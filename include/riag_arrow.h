#pragma once

#include "rust/cxx.h"
#ifdef RIAG_BUNDLED
#include "main/riag.h"
#else
#include <riag.hpp>
#endif

namespace riag_arrow {

ArrowSchema query_result_get_arrow_schema(const riag::main::QueryResult& result);
bool query_result_has_next_arrow_chunk(riag::main::QueryResult& result);
ArrowArray query_result_get_next_arrow_chunk(riag::main::QueryResult& result, uint64_t chunkSize);
ArrowArray query_result_get_csr_indptr(const riag::main::QueryResult& result);
ArrowArray query_result_get_csr_indices(const riag::main::QueryResult& result);
ArrowArray query_result_get_csr_edge_ids(const riag::main::QueryResult& result);
bool query_result_has_csr_edge_ids(const riag::main::QueryResult& result);

} // namespace riag_arrow
