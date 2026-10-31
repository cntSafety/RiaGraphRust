//! Bindings to RiaG: an in-process property graph database management system built for query speed and scalability.
//!
//! ## Example Usage
//! ```
//! use riag::{Database, SystemConfig, Connection};
//! # use anyhow::Error;
//!
//! # fn main() -> Result<(), Error> {
//! # let temp_dir = tempfile::tempdir()?;
//! # let path = temp_dir.path().join("testdb");
//! let db = Database::new(path, SystemConfig::default())?;
//! let conn = Connection::new(&db)?;
//! conn.query("CREATE NODE TABLE Person(name STRING, age INT64, PRIMARY KEY(name));")?;
//! conn.query("CREATE (:Person {name: 'Alice', age: 25});")?;
//! conn.query("CREATE (:Person {name: 'Bob', age: 30});")?;
//!
//! let mut result = conn.query("MATCH (a:Person) RETURN a.name AS NAME, a.age AS AGE;")?;
//! println!("{}", result);
//! # temp_dir.close()?;
//! # Ok(())
//! # }
//! ```
//! ## Building
//!
//! By default, the build downloads a precompiled static `libriag` archive and links it into this
//! crate. If a precompiled archive is unavailable, the RiaG C++ library will be compiled from
//! source and statically linked.
//!
//! If you want to instead link against a pre-built version of the library, the following environment
//! variables can be used to configure the build process:
//!
//! - `RIAG_SHARED`: If set, link dynamically instead of statically
//! - `RIAG_SOURCE_DIR`: Directory of a RiaG source checkout to use when falling back to a source
//!   build. Defaults to `../riagraph` when present.
//! - `RIAG_INCLUDE_DIR`: Directory of RiaG's headers
//! - `RIAG_LIBRARY_DIR`: Directory containing RiaG's pre-built libraries.
//! - `RIAG_BUILD_FROM_SOURCE` or `RIAG_RUST_BUILD_FROM_SOURCE`: If set, skip downloading a
//!   precompiled `libriag` and build from source.
//!
//! Example:
//! ```bash
//! riag_prebuilt_dir=/tmp/riag # pre-built RiaG
//! riag_prebuilt_dir=/path_to_riag_source/build/release/src # RiaG built from source
//! export RIAG_LIBRARY_DIR="riag_prebuilt_dir"
//! export RIAG_INCLUDE_DIR="riag_prebuilt_dir"
//! export RIAG_SHARED=1
//! ```
//! On macOS:
//! ```bash
//! brew install riag
//! export RIAG_LIBRARY_DIR=/opt/homebrew/lib
//! export RIAG_INCLUDE_DIR=/opt/homebrew/include
//! export RIAG_SHARED=1
//! ```
//!
//! - `RIAG_PREBUILT_CACHE_DIR`: Directory to keep downloaded prebuilt archives in, shared across
//!   builds. By default they live under the build's `OUT_DIR`.
//! - `RIAG_LOCALIZE_BUNDLED_SYMBOLS`: On Linux static builds, set to `1` to hide the C symbols of
//!   the libraries bundled in the prebuilt archive (zstd, lz4, brotli, simsimd, yyjson, CRoaring)
//!   so they cannot collide with the Rust crates for the same libraries (`zstd-sys`, `lz4-sys`,
//!   `simsimd`, `croaring-sys`, …) at link time. Needs GNU binutils. Leave it unset when the binary
//!   loads extensions that resolve those symbols from the host.
//!
//! ## Bulk loading
//!
//! One `CREATE` or `MERGE` per row costs a full statement each: planning, execution and, outside
//! an explicit transaction, a commit. Measured through this crate on a 4-vCPU host, a single-row
//! relationship `MERGE … SET` costs tens of milliseconds of CPU, so loading a few hundred thousand
//! rows that way takes hours. Load in bulk instead:
//!
//! - From files: `COPY Person FROM 'people.csv'` / `COPY Knows FROM 'knows.parquet'`.
//! - From memory, with the `arrow` feature: register Arrow record batches as tables with
//!   [`Connection::create_arrow_table`] and [`Connection::create_arrow_rel_table`], then
//!   `COPY … FROM` them, or query them directly.
//! - Point lookups by primary key cost about a millisecond of planning each through
//!   [`Connection::prepare`] + [`Connection::execute`]; fetch a neighbourhood with one
//!   `MATCH (a)-[r]->(b) WHERE a.id = $id RETURN …` rather than one lookup per neighbour.
//!
//! ## Memory
//!
//! [`SystemConfig::default`] leaves `buffer_pool_size` at 0, which the engine resolves from the
//! host's physical memory, so an embedding process reserves a large share of RAM regardless of
//! the database's size (about 6 GB resident on a 15 GiB host for a 200,000-edge graph). Set
//! [`SystemConfig::buffer_pool_size`] explicitly when the process shares the machine.
//!
//! ## Using Extensions
//! By default, binaries created using this library will not work with RiaG's
//! extensions (except on Windows/MSVC, where the linker works differently).
//!
//! If you want to use extensions in binaries (binary crates or tests) using this
//! library, you will need to add the following (or a similar command; see
//! [build-scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html#rustc-link-arg))
//! to your build.rs (or create one) so that the binary
//! produced acts like a library that the extension can link with. Not doing this will produce
//! undefined symbol errors when the extension is loaded:
//!
//! ```ignore
//! println!("cargo:rustc-link-arg=-rdynamic");
//! ```

pub use connection::{Connection, PreparedStatement};
pub use database::{Database, SystemConfig};
pub use error::Error;
pub use logical_type::LogicalType;
#[cfg(feature = "arrow")]
pub use query_result::{ArrowIterator, CsrResult};
pub use query_result::{CSVOptions, QueryResult};
pub use value::{InternalID, NodeVal, RelVal, Value};

mod connection;
mod database;
mod error;
mod ffi;
mod logical_type;
mod query_result;
mod value;

/// The version of the RiaG crate as reported by Cargo's `CARGO_PKG_VERSION` environment variable
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
/// The source of the linked RiaG library selected by the build script.
///
/// This is `external` when `RIAG_LIBRARY_DIR`/`RIAG_INCLUDE_DIR` were supplied, `source` when the
/// bundled C++ source was built, or a value such as `run:cntSafety/RiaGraph/25646256977` or
/// `release:cntSafety/RiaGraph/v0.17.0` when a precompiled archive was downloaded.
pub const RIAG_LIBRARY_SOURCE: &str = env!("RIAG_PRECOMPILED_SOURCE");
/// The directory containing the linked precompiled RiaG library, if one was used.
pub const RIAG_LIBRARY_DIR: &str = env!("RIAG_PRECOMPILED_LIBRARY_DIR");

/// Returns the storage version of the RiaG library
pub fn get_storage_version() -> u64 {
    crate::ffi::ffi::get_storage_version()
}

/// Returns the source of the linked RiaG library selected by the build script.
pub fn get_library_source() -> &'static str {
    RIAG_LIBRARY_SOURCE
}

/// Returns the directory containing the linked precompiled RiaG library.
///
/// This returns `None` when RiaG was built from source as part of this crate build.
pub fn get_library_dir() -> Option<&'static str> {
    if RIAG_LIBRARY_DIR.is_empty() {
        None
    } else {
        Some(RIAG_LIBRARY_DIR)
    }
}
