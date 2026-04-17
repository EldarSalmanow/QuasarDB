#include <gtest/gtest.h>
#include <qdb/storage/agregator.h>
#include <qdb/storage/application.h>
#include <qdb/storage/b_star_plus_tree.h>
#include <qdb/storage/connection.h>
#include <qdb/storage/database.h>
#include <qdb/storage/evaluator.h>
#include <qdb/storage/executor.h>
#include <qdb/storage/interner.h>
#include <qdb/storage/journal.h>
#include <qdb/storage/pager.h>
#include <qdb/storage/table.h>

TEST(StorageContracts, HeadersCompile) { SUCCEED(); }
