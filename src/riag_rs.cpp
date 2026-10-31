#include "riag_rs.h"

#include <stdexcept>

using riag::common::ArrayTypeInfo;
using riag::common::Interval;
using riag::common::ListTypeInfo;
using riag::common::LogicalType;
using riag::common::LogicalTypeID;
using riag::common::NodeVal;
using riag::common::RelVal;
using riag::common::StructField;
using riag::common::Value;
using riag::main::Connection;
using riag::main::Database;
using riag::main::SystemConfig;

namespace riag_rs {

std::unique_ptr<QueryParams> new_params() {
    return std::make_unique<QueryParams>();
}

std::unique_ptr<ArrowArrayList> new_arrow_array_list() {
    return std::make_unique<ArrowArrayList>();
}

std::unique_ptr<LogicalType> create_logical_type(riag::common::LogicalTypeID id) {
    return std::make_unique<LogicalType>(id);
}
std::unique_ptr<LogicalType> create_logical_type_list(std::unique_ptr<LogicalType> childType) {
    return std::make_unique<LogicalType>(LogicalType::LIST(std::move(*childType)));
}

std::unique_ptr<LogicalType> create_logical_type_array(std::unique_ptr<LogicalType> childType,
    uint64_t numElements) {
    return std::make_unique<LogicalType>(LogicalType::ARRAY(std::move(*childType), numElements));
}

std::unique_ptr<riag::common::LogicalType> create_logical_type_map(
    std::unique_ptr<LogicalType> keyType, std::unique_ptr<LogicalType> valueType) {
    return std::make_unique<LogicalType>(
        LogicalType::MAP(std::move(*keyType), std::move(*valueType)));
}

std::unique_ptr<LogicalType> logical_type_get_list_child_type(const LogicalType& logicalType) {
    return std::make_unique<LogicalType>(riag::common::ListType::getChildType(logicalType).copy());
}
std::unique_ptr<LogicalType> logical_type_get_array_child_type(const LogicalType& logicalType) {
    return std::make_unique<LogicalType>(riag::common::ArrayType::getChildType(logicalType).copy());
}
uint64_t logical_type_get_array_num_elements(const LogicalType& logicalType) {
    return riag::common::ArrayType::getNumElements(logicalType);
}

rust::Vec<rust::String> logical_type_get_struct_field_names(
    const riag::common::LogicalType& value) {
    rust::Vec<rust::String> names;
    for (auto name : riag::common::StructType::getFieldNames(value)) {
        names.push_back(name);
    }
    return names;
}

std::unique_ptr<std::vector<riag::common::LogicalType>> logical_type_get_struct_field_types(
    const riag::common::LogicalType& value) {
    std::vector<riag::common::LogicalType> result;
    for (const auto& type : riag::common::StructType::getFieldTypes(value)) {
        result.push_back(type->copy());
    }
    return std::make_unique<std::vector<LogicalType>>(std::move(result));
}

std::unique_ptr<Database> new_database(std::string_view databasePath, uint64_t bufferPoolSize,
    uint64_t maxNumThreads, bool enableCompression, bool readOnly, uint64_t maxDBSize,
    bool autoCheckpoint, int64_t checkpointThreshold, bool throwOnWalReplayFailure,
    bool enableChecksums, bool enableMultiWrites) {
    auto systemConfig = SystemConfig();
    if (bufferPoolSize > 0) {
        systemConfig.bufferPoolSize = bufferPoolSize;
    }
    if (maxNumThreads > 0) {
        systemConfig.maxNumThreads = maxNumThreads;
    }
    systemConfig.readOnly = readOnly;
    systemConfig.enableCompression = enableCompression;
    if (maxDBSize != -1u) {
        systemConfig.maxDBSize = maxDBSize;
    }
    systemConfig.autoCheckpoint = autoCheckpoint;
    if (checkpointThreshold >= 0) {
        systemConfig.checkpointThreshold = checkpointThreshold;
    }
    systemConfig.throwOnWalReplayFailure = throwOnWalReplayFailure;
    systemConfig.enableChecksums = enableChecksums;
    systemConfig.enableMultiWrites = enableMultiWrites;
    return std::make_unique<Database>(databasePath, systemConfig);
}

std::unique_ptr<riag::main::Connection> database_connect(riag::main::Database& database) {
    return std::make_unique<Connection>(&database);
}

std::unique_ptr<riag::main::QueryResult> connection_execute(riag::main::Connection& connection,
    riag::main::PreparedStatement& query, std::unique_ptr<QueryParams> params) {
    return connection.executeWithParams(&query, std::move(params->inputParams));
}

std::unique_ptr<riag::main::QueryResult> connection_create_arrow_table(
    riag::main::Connection& connection, std::string_view tableName, ArrowSchema schema,
    std::unique_ptr<ArrowArrayList> arrays) {
    riag_connection capiConnection{&connection};
    riag_query_result capiResult{};
    (void)riag_connection_create_arrow_table(&capiConnection, std::string(tableName).c_str(),
        &schema, arrays->arrays.data(), arrays->arrays.size(), &capiResult);
    if (capiResult._query_result == nullptr) {
        auto error = riag_get_last_error();
        std::string message = error == nullptr ? "Failed to create Arrow table" : error;
        if (error != nullptr) {
            riag_destroy_string(error);
        }
        throw std::runtime_error(message);
    }
    return std::unique_ptr<riag::main::QueryResult>(
        static_cast<riag::main::QueryResult*>(capiResult._query_result));
}

std::unique_ptr<riag::main::QueryResult> connection_create_arrow_rel_table(
    riag::main::Connection& connection, std::string_view tableName, std::string_view srcTableName,
    std::string_view dstTableName, ArrowSchema schema, std::unique_ptr<ArrowArrayList> arrays) {
    riag_connection capiConnection{&connection};
    riag_query_result capiResult{};
    auto tableNameString = std::string(tableName);
    auto srcTableNameString = std::string(srcTableName);
    auto dstTableNameString = std::string(dstTableName);
    (void)riag_connection_create_arrow_rel_table(&capiConnection, tableNameString.c_str(),
        srcTableNameString.c_str(), dstTableNameString.c_str(), &schema, arrays->arrays.data(),
        arrays->arrays.size(), &capiResult);
    if (capiResult._query_result == nullptr) {
        auto error = riag_get_last_error();
        std::string message =
            error == nullptr ? "Failed to create Arrow relationship table" : error;
        if (error != nullptr) {
            riag_destroy_string(error);
        }
        throw std::runtime_error(message);
    }
    return std::unique_ptr<riag::main::QueryResult>(
        static_cast<riag::main::QueryResult*>(capiResult._query_result));
}

std::unique_ptr<riag::main::QueryResult> connection_create_arrow_rel_table_csr(
    riag::main::Connection& connection, std::string_view tableName, std::string_view srcTableName,
    std::string_view dstTableName, ArrowSchema indicesSchema,
    std::unique_ptr<ArrowArrayList> indicesArrays, ArrowSchema indptrSchema,
    std::unique_ptr<ArrowArrayList> indptrArrays, std::string_view dstColName) {
    riag_connection capiConnection{&connection};
    riag_query_result capiResult{};
    auto tableNameString = std::string(tableName);
    auto srcTableNameString = std::string(srcTableName);
    auto dstTableNameString = std::string(dstTableName);
    auto dstColNameString = std::string(dstColName);
    (void)riag_connection_create_arrow_rel_table_csr(&capiConnection, tableNameString.c_str(),
        srcTableNameString.c_str(), dstTableNameString.c_str(), &indicesSchema,
        indicesArrays->arrays.data(), indicesArrays->arrays.size(), &indptrSchema,
        indptrArrays->arrays.data(), indptrArrays->arrays.size(), dstColNameString.c_str(),
        &capiResult);
    if (capiResult._query_result == nullptr) {
        auto error = riag_get_last_error();
        std::string message =
            error == nullptr ? "Failed to create Arrow CSR relationship table" : error;
        if (error != nullptr) {
            riag_destroy_string(error);
        }
        throw std::runtime_error(message);
    }
    return std::unique_ptr<riag::main::QueryResult>(
        static_cast<riag::main::QueryResult*>(capiResult._query_result));
}

std::unique_ptr<riag::main::QueryResult> connection_drop_arrow_table(
    riag::main::Connection& connection, std::string_view tableName) {
    riag_connection capiConnection{&connection};
    riag_query_result capiResult{};
    (void)riag_connection_drop_arrow_table(&capiConnection, std::string(tableName).c_str(),
        &capiResult);
    if (capiResult._query_result == nullptr) {
        auto error = riag_get_last_error();
        std::string message = error == nullptr ? "Failed to drop Arrow table" : error;
        if (error != nullptr) {
            riag_destroy_string(error);
        }
        throw std::runtime_error(message);
    }
    return std::unique_ptr<riag::main::QueryResult>(
        static_cast<riag::main::QueryResult*>(capiResult._query_result));
}

rust::String prepared_statement_error_message(const riag::main::PreparedStatement& statement) {
    return rust::String(statement.getErrorMessage());
}

rust::String query_result_to_string(const riag::main::QueryResult& result) {
    return rust::String(result.toString());
}

rust::String query_result_get_error_message(const riag::main::QueryResult& result) {
    return rust::String(result.getErrorMessage());
}

double query_result_get_compiling_time(const riag::main::QueryResult& result) {
    return result.getQuerySummary()->getCompilingTime();
}
double query_result_get_execution_time(const riag::main::QueryResult& result) {
    return result.getQuerySummary()->getExecutionTime();
}

std::unique_ptr<std::vector<riag::common::LogicalType>> query_result_column_data_types(
    const riag::main::QueryResult& query_result) {
    return std::make_unique<std::vector<riag::common::LogicalType>>(
        query_result.getColumnDataTypes());
}
rust::Vec<rust::String> query_result_column_names(const riag::main::QueryResult& query_result) {
    rust::Vec<rust::String> names;
    for (auto name : query_result.getColumnNames()) {
        names.push_back(name);
    }
    return names;
}

rust::String node_value_get_label_name(const riag::common::Value& val) {
    return rust::String(riag::common::NodeVal::getLabelVal(&val)->getValue<std::string>());
}

rust::String rel_value_get_label_name(const riag::common::Value& val) {
    return rust::String(riag::common::RelVal::getLabelVal(&val)->getValue<std::string>());
}

size_t node_value_get_num_properties(const Value& value) {
    return NodeVal::getNumProperties(&value);
}
size_t rel_value_get_num_properties(const Value& value) {
    return RelVal::getNumProperties(&value);
}

rust::String node_value_get_property_name(const Value& value, size_t index) {
    return rust::String(NodeVal::getPropertyName(&value, index));
}
rust::String rel_value_get_property_name(const Value& value, size_t index) {
    return rust::String(RelVal::getPropertyName(&value, index));
}

const riag::common::Value& node_value_get_property_value(const Value& value, size_t index) {
    return *NodeVal::getPropertyVal(&value, index);
}
const riag::common::Value& rel_value_get_property_value(const Value& value, size_t index) {
    return *RelVal::getPropertyVal(&value, index);
}

const Value& node_value_get_node_id(const riag::common::Value& val) {
    return *riag::common::NodeVal::getNodeIDVal(&val);
}

const Value& rel_value_get_src_id(const riag::common::Value& val) {
    return *riag::common::RelVal::getSrcNodeIDVal(&val);
}
std::array<uint64_t, 2> rel_value_get_dst_id(const riag::common::Value& val) {
    auto internalID =
        riag::common::RelVal::getDstNodeIDVal(&val)->getValue<riag::common::internalID_t>();
    return std::array{internalID.offset, internalID.tableID};
}

rust::String value_to_string(const riag::common::Value& val) {
    return rust::String(val.toString());
}

uint32_t flat_tuple_len(const riag::processor::FlatTuple& flatTuple) {
    return flatTuple.len();
}
const riag::common::Value& flat_tuple_get_value(const riag::processor::FlatTuple& flatTuple,
    uint32_t index) {
    return flatTuple[index];
}

const std::string& value_get_string(const riag::common::Value& value) {
    return value.strVal;
}
int64_t value_get_interval_secs(const riag::common::Value& value) {
    auto interval = value.getValue<riag::common::interval_t>();
    return (interval.months * Interval::DAYS_PER_MONTH + interval.days) * Interval::HOURS_PER_DAY *
               Interval::MINS_PER_HOUR * Interval::SECS_PER_MINUTE
           // Include extra microseconds with the seconds
           + interval.micros / Interval::MICROS_PER_SEC;
}
int32_t value_get_interval_micros(const riag::common::Value& value) {
    auto interval = value.getValue<riag::common::interval_t>();
    return interval.micros % Interval::MICROS_PER_SEC;
}
int32_t value_get_date_days(const riag::common::Value& value) {
    return value.getValue<riag::common::date_t>().days;
}
int64_t value_get_timestamp_micros(const riag::common::Value& value) {
    return value.getValue<riag::common::timestamp_t>().value;
}
int64_t value_get_timestamp_ns(const riag::common::Value& value) {
    return value.getValue<riag::common::timestamp_ns_t>().value;
}
int64_t value_get_timestamp_ms(const riag::common::Value& value) {
    return value.getValue<riag::common::timestamp_ms_t>().value;
}
int64_t value_get_timestamp_sec(const riag::common::Value& value) {
    return value.getValue<riag::common::timestamp_sec_t>().value;
}
int64_t value_get_timestamp_tz(const riag::common::Value& value) {
    return value.getValue<riag::common::timestamp_tz_t>().value;
}
std::array<uint64_t, 2> value_get_int128_t(const riag::common::Value& value) {
    auto int128 = value.getValue<riag::common::int128_t>();
    auto int128_high = static_cast<uint64_t>(int128.high);
    return std::array{int128_high, int128.low};
}

std::array<uint64_t, 2> value_get_internal_id(const riag::common::Value& value) {
    auto internalID = value.getValue<riag::common::internalID_t>();
    return std::array{internalID.offset, internalID.tableID};
}

uint32_t value_get_children_size(const riag::common::Value& value) {
    return riag::common::NestedVal::getChildrenSize(&value);
}

const Value& value_get_child(const riag::common::Value& value, uint32_t index) {
    return *riag::common::NestedVal::getChildVal(&value, index);
}

riag::common::LogicalTypeID value_get_data_type_id(const riag::common::Value& value) {
    return value.getDataType().getLogicalTypeID();
}

const LogicalType& value_get_data_type(const riag::common::Value& value) {
    return value.getDataType();
}

std::unique_ptr<riag::common::Value> create_value_string(LogicalTypeID typ,
    const rust::Slice<const unsigned char> value) {
    return std::make_unique<riag::common::Value>(LogicalType(typ),
        std::string((const char*)value.data(), value.size()));
}
std::unique_ptr<riag::common::Value> create_value_timestamp(const int64_t timestamp) {
    return std::make_unique<riag::common::Value>(riag::common::timestamp_t(timestamp));
}
std::unique_ptr<riag::common::Value> create_value_timestamp_tz(const int64_t timestamp) {
    return std::make_unique<riag::common::Value>(riag::common::timestamp_tz_t(timestamp));
}
std::unique_ptr<riag::common::Value> create_value_timestamp_ns(const int64_t timestamp) {
    return std::make_unique<riag::common::Value>(riag::common::timestamp_ns_t(timestamp));
}
std::unique_ptr<riag::common::Value> create_value_timestamp_ms(const int64_t timestamp) {
    return std::make_unique<riag::common::Value>(riag::common::timestamp_ms_t(timestamp));
}
std::unique_ptr<riag::common::Value> create_value_timestamp_sec(const int64_t timestamp) {
    return std::make_unique<riag::common::Value>(riag::common::timestamp_sec_t(timestamp));
}
std::unique_ptr<riag::common::Value> create_value_interval(const int32_t months, const int32_t days,
    const int64_t micros) {
    return std::make_unique<riag::common::Value>(riag::common::interval_t(months, days, micros));
}
std::unique_ptr<riag::common::Value> create_value_null(
    std::unique_ptr<riag::common::LogicalType> typ) {
    return std::make_unique<riag::common::Value>(
        riag::common::Value::createNullValue(std::move(*typ)));
}
std::unique_ptr<riag::common::Value> create_value_internal_id(uint64_t offset, uint64_t table) {
    return std::make_unique<riag::common::Value>(riag::common::internalID_t(offset, table));
}
std::unique_ptr<riag::common::Value> create_value_int128_t(int64_t high, uint64_t low) {
    return std::make_unique<riag::common::Value>(riag::common::int128_t(low, high));
}

std::unique_ptr<riag::common::Value> get_list_value(std::unique_ptr<riag::common::LogicalType> typ,
    std::unique_ptr<ValueListBuilder> value) {
    return std::make_unique<riag::common::Value>(std::move(*typ), std::move(value->values));
}

std::unique_ptr<ValueListBuilder> create_list() {
    return std::make_unique<ValueListBuilder>();
}

std::unique_ptr<TypeListBuilder> create_type_list() {
    return std::make_unique<TypeListBuilder>();
}

const riag::common::Value& recursive_rel_get_nodes(const riag::common::Value& val) {
    return *riag::common::RecursiveRelVal::getNodes(&val);
}
const riag::common::Value& recursive_rel_get_rels(const riag::common::Value& val) {
    return *riag::common::RecursiveRelVal::getRels(&val);
}

} // namespace riag_rs
