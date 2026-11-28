#ifndef SQLNX_SQLITE_HPP
#define SQLNX_SQLITE_HPP

/// \file
/// Everything sqlnx offers. Include this, or the individual headers.

#include "sqlnx/fields.hpp"
#include "sqlnx/sqlite/aggregate.hpp"
#include "sqlnx/sqlite/backup.hpp"
#include "sqlnx/sqlite/blob_stream.hpp"
#include "sqlnx/sqlite/chain.hpp"
#include "sqlnx/sqlite/collation.hpp"
#include "sqlnx/sqlite/config.hpp"
#include "sqlnx/sqlite/cstring_view.hpp"
#include "sqlnx/sqlite/database.hpp"
#include "sqlnx/sqlite/error.hpp"
#include "sqlnx/sqlite/expected.hpp"
#include "sqlnx/sqlite/function.hpp"
#include "sqlnx/sqlite/hooks.hpp"
#include "sqlnx/sqlite/native/backup.hpp"
#include "sqlnx/sqlite/native/blob_handle.hpp"
#include "sqlnx/sqlite/native/collation_callback.hpp"
#include "sqlnx/sqlite/native/connection.hpp"
#include "sqlnx/sqlite/native/context.hpp"
#include "sqlnx/sqlite/native/extended_code.hpp"
#include "sqlnx/sqlite/native/flags.hpp"
#include "sqlnx/sqlite/native/hook_callbacks.hpp"
#include "sqlnx/sqlite/native/result.hpp"
#include "sqlnx/sqlite/native/statement.hpp"
#include "sqlnx/sqlite/native/value.hpp"
#include "sqlnx/sqlite/record_binder.hpp"
#include "sqlnx/sqlite/record_builder.hpp"
#include "sqlnx/sqlite/record_traits.hpp"
#include "sqlnx/sqlite/registrations.hpp"
#include "sqlnx/sqlite/results.hpp"
#include "sqlnx/sqlite/rows.hpp"
#include "sqlnx/sqlite/statement.hpp"
#include "sqlnx/sqlite/transaction.hpp"
#include "sqlnx/sqlite/value.hpp"
#include "sqlnx/sqlite/value_bytes.hpp"
#include "sqlnx/sqlite/value_char8.hpp"
#include "sqlnx/sqlite/zeroblob.hpp"

#endif // SQLNX_SQLITE_HPP
