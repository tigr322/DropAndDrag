#pragma once

// migrations.hpp — run_migrations() declaration for db.cpp.
// All migration logic lives in migrations.cpp.


struct sqlite3;

namespace dd {

void run_migrations(sqlite3* db);

} // namespace dd
