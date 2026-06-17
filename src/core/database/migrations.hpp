#pragma once

struct sqlite3;

namespace dd {

void run_migrations(sqlite3* db);

} // namespace dd
