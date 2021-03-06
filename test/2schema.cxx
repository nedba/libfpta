/*
 * Copyright 2016-2018 libfpta authors: please see AUTHORS file.
 *
 * This file is part of libfpta, aka "Fast Positive Tables".
 *
 * libfpta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libfpta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libfpta.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fpta_test.h"

static const char testdb_name[] = "ut_schema.fpta";
static const char testdb_name_lck[] = "ut_schema.fpta" MDBX_LOCK_SUFFIX;

TEST(Schema, Trivia) {
  /* Тривиальный тест создания/заполнения описания колонок таблицы.
   *
   * Сценарий:
   *  - создаем/инициализируем описание колонок.
   *  - пробуем добавить несколько некорректных колонок,
   *    с плохими: именем, индексом, типом.
   *  - добавляем несколько корректных описаний колонок.
   *
   * Тест НЕ перебирает все возможные комбинации, а только некоторые.
   * Такой относительно полный перебор происходит автоматически при
   * тестировании индексов и курсоров. */
  fpta_column_set def;
  fpta_column_set_init(&def);
  EXPECT_NE(FPTA_SUCCESS, fpta_column_set_validate(&def));

  EXPECT_EQ(FPTA_ENAME,
            fpta_column_describe("", fptu_cstr,
                                 fpta_primary_unique_ordered_obverse, &def));
  EXPECT_EQ(FPTA_EINVAL,
            fpta_column_describe("column_a", fptu_cstr,
                                 fpta_primary_unique_ordered_obverse, nullptr));

  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("column_a", fptu_uint64,
                                 fpta_primary_unique_ordered_reverse, &def));
  EXPECT_EQ(FPTA_ETYPE,
            fpta_column_describe("column_a", fptu_null,
                                 fpta_primary_unique_ordered_obverse, &def));

  /* Валидны все комбинации, в которых установлен хотя-бы один из флажков
   * fpta_index_fordered или fpta_index_fobverse. Другим словами,
   * не может быть unordered индекса со сравнением ключей
   * в обратном порядке. Однако, также допустим fpta_index_none.
   *
   * Поэтому внутри диапазона остаются только две недопустимые комбинации,
   * которые и проверяем. */
  EXPECT_EQ(FPTA_EFLAG, fpta_column_describe("column_a", fptu_cstr,
                                             fpta_index_funique, &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe(
                "column_a", fptu_cstr,
                (fpta_index_type)(fpta_index_fsecondary | fpta_index_funique),
                &def));
  EXPECT_EQ(FPTA_EFLAG, fpta_column_describe("column_a", fptu_cstr,
                                             (fpta_index_type)-1, &def));
  EXPECT_EQ(
      FPTA_EFLAG,
      fpta_column_describe(
          "column_a", fptu_cstr,
          (fpta_index_type)(fpta_index_funique + fpta_index_fordered +
                            fpta_index_fobverse + fpta_index_fsecondary + 1),
          &def));

  EXPECT_EQ(FPTA_OK,
            fpta_column_describe("column_a", fptu_cstr,
                                 fpta_primary_unique_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_validate(&def));

  EXPECT_EQ(FPTA_EEXIST,
            fpta_column_describe("column_b", fptu_cstr,
                                 fpta_primary_unique_ordered_obverse, &def));
  EXPECT_EQ(FPTA_EEXIST, fpta_column_describe(
                             "column_a", fptu_cstr,
                             fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_validate(&def));
  EXPECT_EQ(FPTA_EEXIST, fpta_column_describe(
                             "COLUMN_A", fptu_cstr,
                             fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_validate(&def));

  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "column_b", fptu_cstr,
                         fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_validate(&def));

  EXPECT_EQ(FPTA_EEXIST, fpta_column_describe(
                             "column_b", fptu_fp64,
                             fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_EEXIST, fpta_column_describe(
                             "COLUMN_B", fptu_fp64,
                             fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_validate(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "column_c", fptu_uint16,
                         fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_validate(&def));

  EXPECT_EQ(FPTA_EEXIST, fpta_column_describe(
                             "column_A", fptu_int32,
                             fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_EEXIST, fpta_column_describe(
                             "Column_b", fptu_datetime,
                             fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_EEXIST, fpta_column_describe(
                             "coLumn_c", fptu_opaque,
                             fpta_secondary_withdups_ordered_obverse, &def));

  EXPECT_EQ(FPTA_OK, fpta_column_set_destroy(&def));
  EXPECT_NE(FPTA_OK, fpta_column_set_validate(&def));
  EXPECT_EQ(FPTA_EINVAL, fpta_column_set_destroy(&def));
}

TEST(Schema, Base) {
  /* Базовый тест создания таблицы.
   *
   * Сценарий:
   *  - открываем базу в режиме неизменяемой схемы и пробуем начать
   *    транзакцию уровня изменения схемы.
   *  - открываем базу в режиме изменяемой схемы.
   *  - создаем и заполняем описание колонок.
   *  - создаем таблицу по сформированному описанию колонок.
   *  - затем в другой транзакции проверяем, что у созданной таблицы
   *    есть соответствующие колонки.
   *  - в очередной транзакции создаем еще одну таблицу
   *    и обновляем описание первой.
   *  - после в другой транзакции удаляем созданную таблицу,
   *    а также пробуем удалить несуществующую.
   *
   * Тест НЕ перебирает комбинации. Некий относительно полный перебор
   * происходит автоматически при тестировании индексов и курсоров. */
  if (REMOVE_FILE(testdb_name) != 0) {
    ASSERT_EQ(ENOENT, errno);
  }
  if (REMOVE_FILE(testdb_name_lck) != 0) {
    ASSERT_EQ(ENOENT, errno);
  }

  fpta_db *db = nullptr;
  /* открываем базу в режиме неизменяемой схемы */
  EXPECT_EQ(FPTA_SUCCESS,
            fpta_db_open(testdb_name, fpta_weak, fpta_regime_default, 0644, 1,
                         false, &db));
  ASSERT_NE(nullptr, db);

  /* пробуем начать транзакцию изменения схемы в базе с неизменяемой схемой */
  fpta_txn *txn = (fpta_txn *)&txn;
  EXPECT_EQ(EPERM, fpta_transaction_begin(db, fpta_schema, &txn));
  ASSERT_EQ(nullptr, txn);
  ASSERT_EQ(FPTA_SUCCESS, fpta_db_close(db));

  //------------------------------------------------------------------------

  /* повторно открываем базу с возможностью изменять схему */
  EXPECT_EQ(FPTA_SUCCESS,
            fpta_db_open(testdb_name, fpta_weak, fpta_regime_default, 0644, 1,
                         true, &db));
  ASSERT_NE(nullptr, db);

  // формируем описание колонок для первой таблицы
  fpta_column_set def;
  fpta_column_set_init(&def);
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe("pk_str_uniq", fptu_cstr,
                                 fpta_primary_unique_ordered_obverse, &def));
  /* LY: было упущение, порядок колонок с одинаковыми индексами/опциями
   * может отличаться от добавленного */
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "first_uint", fptu_uint64,
                         fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe("second_fp", fptu_fp64,
                                          fpta_index_none, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_validate(&def));

  // формируем описание колонок для второй таблицы
  fpta_column_set def2;
  fpta_column_set_init(&def2);
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe("x", fptu_cstr,
                                 fpta_primary_unique_ordered_obverse, &def2));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "y", fptu_cstr,
                         fpta_secondary_withdups_ordered_obverse, &def2));
  EXPECT_EQ(FPTA_OK, fpta_column_set_validate(&def2));

  //------------------------------------------------------------------------
  // создаем первую таблицу в отдельной транзакции
  EXPECT_EQ(FPTA_EINVAL, fpta_transaction_begin(db, fpta_read, nullptr));
  EXPECT_EQ(FPTA_EFLAG, fpta_transaction_begin(db, (fpta_level)0, &txn));
  EXPECT_EQ(nullptr, txn);
  EXPECT_EQ(FPTA_OK, fpta_transaction_begin(db, fpta_schema, &txn));
  ASSERT_NE(nullptr, txn);

  EXPECT_EQ(FPTA_OK, fpta_table_create(txn, "table_1", &def));

  fpta_schema_info schema_info;
  EXPECT_EQ(FPTA_OK, fpta_schema_fetch(txn, &schema_info));
  EXPECT_EQ(1u, schema_info.tables_count);
  EXPECT_EQ(FPTA_OK, fpta_schema_destroy(&schema_info));

  EXPECT_EQ(FPTA_OK, fpta_transaction_end(txn, false));
  txn = nullptr;

  EXPECT_EQ(FPTA_OK, fpta_column_set_destroy(&def));
  EXPECT_NE(FPTA_OK, fpta_column_set_validate(&def));

  //------------------------------------------------------------------------
  // проверяем наличие первой таблицы

  fpta_name table, col_pk, col_a, col_b, probe_get;
  fpta_pollute(&table, sizeof(table), 0); // чтобы valrind не ругался
  EXPECT_GT(0, fpta_table_column_count(&table));
  EXPECT_EQ(FPTA_EINVAL, fpta_table_column_get(&table, 0, &probe_get));

  EXPECT_EQ(FPTA_OK, fpta_table_init(&table, "tAbLe_1"));
  EXPECT_EQ(FPTA_OK, fpta_table_init(&table, "table_1"));
  EXPECT_EQ(FPTA_OK, fpta_column_init(&table, &col_pk, "pk_str_uniq"));
  EXPECT_EQ(FPTA_OK, fpta_column_init(&table, &col_a, "First_Uint"));
  EXPECT_EQ(FPTA_OK, fpta_column_init(&table, &col_b, "second_FP"));

  EXPECT_GT(0, fpta_table_column_count(&table));
  EXPECT_EQ(FPTA_EINVAL, fpta_table_column_get(&table, 0, &probe_get));

  EXPECT_EQ(FPTA_OK, fpta_transaction_begin(db, fpta_read, &txn));
  ASSERT_NE(nullptr, txn);

  EXPECT_EQ(FPTA_OK, fpta_name_refresh_couple(txn, &table, &col_pk));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_a));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_b));

  EXPECT_EQ(3, fpta_table_column_count(&table));
  EXPECT_EQ(FPTA_OK, fpta_table_column_get(&table, 0, &probe_get));
  EXPECT_EQ(0, memcmp(&probe_get, &col_pk, sizeof(fpta_name)));
  EXPECT_EQ(FPTA_OK, fpta_table_column_get(&table, 1, &probe_get));
  EXPECT_EQ(0, memcmp(&probe_get, &col_a, sizeof(fpta_name)));
  EXPECT_EQ(FPTA_OK, fpta_table_column_get(&table, 2, &probe_get));
  EXPECT_EQ(0, memcmp(&probe_get, &col_b, sizeof(fpta_name)));
  EXPECT_EQ(FPTA_NODATA, fpta_table_column_get(&table, 3, &probe_get));

  EXPECT_EQ(fptu_cstr, fpta_shove2type(col_pk.shove));
  EXPECT_EQ(fpta_primary_unique_ordered_obverse, fpta_name_colindex(&col_pk));
  EXPECT_EQ(fptu_cstr, fpta_name_coltype(&col_pk));
  EXPECT_EQ(0u, col_pk.column.num);

  EXPECT_EQ(fptu_uint64, fpta_shove2type(col_a.shove));
  EXPECT_EQ(fpta_secondary_withdups_ordered_obverse,
            fpta_name_colindex(&col_a));
  EXPECT_EQ(fptu_uint64, fpta_name_coltype(&col_a));
  EXPECT_EQ(1u, col_a.column.num);

  EXPECT_EQ(fptu_fp64, fpta_shove2type(col_b.shove));
  EXPECT_EQ(fpta_index_none, fpta_name_colindex(&col_b));
  EXPECT_EQ(fptu_fp64, fpta_name_coltype(&col_b));
  EXPECT_EQ(2u, col_b.column.num);

  // получаем описание схемы, проверяем кол-во таблиц и освобождаем
  EXPECT_EQ(FPTA_OK, fpta_schema_fetch(txn, &schema_info));
  EXPECT_EQ(1u, schema_info.tables_count);
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &schema_info.tables_names[0]));
  EXPECT_EQ(FPTA_OK, fpta_schema_destroy(&schema_info));

  EXPECT_EQ(FPTA_OK, fpta_transaction_end(txn, false));
  txn = nullptr;

  //------------------------------------------------------------------------
  // создаем вторую таблицу в отдельной транзакции
  EXPECT_EQ(FPTA_EINVAL, fpta_transaction_begin(db, fpta_read, nullptr));
  EXPECT_EQ(FPTA_EFLAG, fpta_transaction_begin(db, (fpta_level)0, &txn));
  EXPECT_EQ(nullptr, txn);
  EXPECT_EQ(FPTA_OK, fpta_transaction_begin(db, fpta_schema, &txn));
  ASSERT_NE(nullptr, txn);

  EXPECT_EQ(FPTA_OK, fpta_table_create(txn, "table_2", &def2));
  EXPECT_EQ(FPTA_OK, fpta_schema_fetch(txn, &schema_info));
  EXPECT_EQ(2u, schema_info.tables_count);
  EXPECT_EQ(FPTA_OK, fpta_schema_destroy(&schema_info));

  EXPECT_EQ(FPTA_OK, fpta_transaction_end(txn, false));
  txn = nullptr;

  EXPECT_EQ(FPTA_OK, fpta_column_set_destroy(&def2));
  EXPECT_NE(FPTA_OK, fpta_column_set_validate(&def2));

  //------------------------------------------------------------------------
  // проверяем наличие второй таблицы и обновляем описание первой
  fpta_name table2, col_x, col_y;
  EXPECT_EQ(FPTA_OK, fpta_table_init(&table2, "table_2"));
  EXPECT_EQ(FPTA_OK, fpta_column_init(&table2, &col_x, "x"));
  EXPECT_EQ(FPTA_OK, fpta_column_init(&table2, &col_y, "y"));
  EXPECT_EQ(FPTA_OK, fpta_transaction_begin(db, fpta_read, &txn));
  ASSERT_NE(nullptr, txn);

  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &table2));
  EXPECT_EQ(2, fpta_table_column_count(&table2));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_x));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_y));

  EXPECT_EQ(FPTA_OK, fpta_name_refresh_couple(txn, &table, &col_pk));
  EXPECT_EQ(3, fpta_table_column_count(&table));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_a));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_b));

  EXPECT_EQ(FPTA_OK, fpta_transaction_end(txn, false));
  txn = nullptr;

  //------------------------------------------------------------------------
  // в отдельной транзакции удаляем первую таблицу
  EXPECT_EQ(FPTA_OK, fpta_transaction_begin(db, fpta_schema, &txn));
  ASSERT_NE(nullptr, txn);

  EXPECT_EQ(FPTA_OK, fpta_schema_fetch(txn, &schema_info));
  EXPECT_EQ(2u, schema_info.tables_count);
  EXPECT_EQ(FPTA_OK, fpta_schema_destroy(&schema_info));

  // удаляем первую таблицу
  EXPECT_EQ(FPTA_OK, fpta_table_drop(txn, "Table_1"));
  EXPECT_EQ(FPTA_OK, fpta_schema_fetch(txn, &schema_info));
  EXPECT_EQ(1u, schema_info.tables_count);
  EXPECT_EQ(FPTA_OK, fpta_schema_destroy(&schema_info));

  // пробуем удалить несуществующую таблицу
  EXPECT_EQ(FPTA_NOTFOUND, fpta_table_drop(txn, "table_xyz"));
  EXPECT_EQ(FPTA_OK, fpta_schema_fetch(txn, &schema_info));
  EXPECT_EQ(1u, schema_info.tables_count);
  EXPECT_EQ(FPTA_OK, fpta_schema_destroy(&schema_info));

  // обновляем описание второй таблицы (внутри транзакции изменения схемы)
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &table2));
  EXPECT_EQ(2, fpta_table_column_count(&table2));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_x));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_y));

  EXPECT_EQ(FPTA_OK, fpta_transaction_end(txn, false));
  txn = nullptr;

  //------------------------------------------------------------------------
  // в отдельной транзакции удаляем вторую таблицу
  EXPECT_EQ(FPTA_OK, fpta_transaction_begin(db, fpta_schema, &txn));
  ASSERT_NE(nullptr, txn);

  EXPECT_EQ(FPTA_OK, fpta_schema_fetch(txn, &schema_info));
  EXPECT_EQ(1u, schema_info.tables_count);
  EXPECT_EQ(FPTA_OK, fpta_schema_destroy(&schema_info));

  // еще раз обновляем описание второй таблицы
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &table2));
  EXPECT_EQ(2, fpta_table_column_count(&table2));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_x));
  EXPECT_EQ(FPTA_OK, fpta_name_refresh(txn, &col_y));

  // удаляем вторую таблицу
  EXPECT_EQ(FPTA_OK, fpta_table_drop(txn, "Table_2"));
  EXPECT_EQ(FPTA_OK, fpta_schema_fetch(txn, &schema_info));
  EXPECT_EQ(0u, schema_info.tables_count);
  EXPECT_EQ(FPTA_OK, fpta_schema_destroy(&schema_info));

  EXPECT_EQ(FPTA_OK, fpta_transaction_end(txn, false));
  txn = nullptr;

  //------------------------------------------------------------------------
  // разрушаем привязанные идентификаторы
  fpta_name_destroy(&table);
  fpta_name_destroy(&col_pk);
  fpta_name_destroy(&col_a);
  fpta_name_destroy(&col_b);
  fpta_name_destroy(&probe_get);

  fpta_name_destroy(&table2);
  fpta_name_destroy(&col_x);
  fpta_name_destroy(&col_y);

  EXPECT_EQ(FPTA_SUCCESS, fpta_db_close(db));
  // пока не удялем файлы чтобы можно было посмотреть и натравить mdbx_chk
  if (false) {
    ASSERT_TRUE(REMOVE_FILE(testdb_name) == 0);
    ASSERT_TRUE(REMOVE_FILE(testdb_name_lck) == 0);
  }
}

TEST(Schema, TriviaWithNullable) {
  /* Тривиальный тест создания/заполнения описания колонок таблицы,
   * включая nullable.
   *
   * Сценарий:
   *  - создаем/инициализируем описание колонок.
   *  - пробуем добавить несколько некорректных nullable колонок.
   *  - добавляем несколько корректных описаний nullable колонок.
   *
   * Тест НЕ перебирает все возможные комбинации, а только некоторые.
   * Такой относительно полный перебор происходит автоматически при
   * тестировании индексов и курсоров. */
  fpta_column_set def;
  fpta_column_set_init(&def);
  EXPECT_NE(FPTA_SUCCESS, fpta_column_set_validate(&def));

  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_int32,
                                 fpta_primary_unique_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_int64,
                                 fpta_primary_unique_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_fp32,
                                 fpta_primary_unique_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_fp64,
                                 fpta_primary_unique_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_datetime,
                                 fpta_primary_unique_ordered_reverse_nullable,
                                 &def));

  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_int32,
                                 fpta_primary_withdups_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_int64,
                                 fpta_primary_withdups_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_fp32,
                                 fpta_primary_withdups_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_fp64,
                                 fpta_primary_withdups_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_datetime,
                                 fpta_primary_withdups_ordered_reverse_nullable,
                                 &def));

  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_int32,
                                 fpta_secondary_unique_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_int64,
                                 fpta_secondary_unique_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_fp32,
                                 fpta_secondary_unique_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_fp64,
                                 fpta_secondary_unique_ordered_reverse_nullable,
                                 &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("col", fptu_datetime,
                                 fpta_secondary_unique_ordered_reverse_nullable,
                                 &def));

  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe(
                "col", fptu_int32,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe(
                "col", fptu_int64,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe(
                "col", fptu_fp32,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe(
                "col", fptu_fp64,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe(
                "col", fptu_datetime,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));

  //------------------------------------------------------------------------

  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo0", fptu_uint16,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdr0", fptu_uint16,
                         fpta_primary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo1", fptu_int32,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo2", fptu_uint32,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdr2", fptu_uint32,
                         fpta_primary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo3", fptu_int64,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo4", fptu_uint64,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdr4", fptu_uint64,
                         fpta_primary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo5", fptu_fp32,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo6", fptu_fp64,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo7", fptu_cstr,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdr7", fptu_cstr,
                         fpta_primary_withdups_ordered_reverse_nullable, &def));

  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo8", fptu_opaque,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdr8", fptu_opaque,
                         fpta_primary_withdups_ordered_reverse_nullable, &def));

  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdo9", fptu_128,
                         fpta_primary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pdr9", fptu_128,
                         fpta_primary_withdups_ordered_reverse_nullable, &def));

  //------------------------------------------------------------------------

  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo0", fptu_uint16,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pur0", fptu_uint16,
                         fpta_primary_unique_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo1", fptu_int32,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo2", fptu_uint32,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pur2", fptu_uint32,
                         fpta_primary_unique_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo3", fptu_int64,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo4", fptu_uint64,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pur4", fptu_uint64,
                         fpta_primary_unique_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo5", fptu_fp32,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo6", fptu_fp64,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo7", fptu_cstr,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pur7", fptu_cstr,
                         fpta_primary_unique_ordered_reverse_nullable, &def));

  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo8", fptu_opaque,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pur8", fptu_opaque,
                         fpta_primary_unique_ordered_reverse_nullable, &def));

  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "puo9", fptu_96,
                         fpta_primary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_set_reset(&def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "pur9", fptu_96,
                         fpta_primary_unique_ordered_reverse_nullable, &def));

  //------------------------------------------------------------------------

  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo0", fptu_uint16,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "sur0", fptu_uint16,
                         fpta_secondary_unique_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo1", fptu_int32,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo2", fptu_uint32,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "sur2", fptu_uint32,
                         fpta_secondary_unique_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo3", fptu_int64,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo4", fptu_uint64,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "sur4", fptu_uint64,
                         fpta_secondary_unique_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo5", fptu_fp32,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo6", fptu_fp64,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo7", fptu_cstr,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "sur7", fptu_cstr,
                         fpta_secondary_unique_ordered_reverse_nullable, &def));

  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo8", fptu_opaque,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "sur8", fptu_opaque,
                         fpta_secondary_unique_ordered_reverse_nullable, &def));

  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "suo9", fptu_160,
                         fpta_secondary_unique_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "sur9", fptu_160,
                         fpta_secondary_unique_ordered_reverse_nullable, &def));

  //------------------------------------------------------------------------

  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo0", fptu_uint16,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdr0", fptu_uint16,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo1", fptu_int32,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo2", fptu_uint32,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdr2", fptu_uint32,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo3", fptu_int64,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo4", fptu_uint64,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdr4", fptu_uint64,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo5", fptu_fp32,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo6", fptu_fp64,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo7", fptu_cstr,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdr7", fptu_cstr,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));

  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo8", fptu_opaque,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdr8", fptu_opaque,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));

  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdo9", fptu_256,
                fpta_secondary_withdups_ordered_obverse_nullable, &def));
  EXPECT_EQ(FPTA_OK,
            fpta_column_describe(
                "sdr9", fptu_256,
                fpta_secondary_withdups_ordered_reverse_nullable, &def));

  //------------------------------------------------------------------------

  EXPECT_EQ(FPTA_OK, fpta_column_set_destroy(&def));
  EXPECT_NE(FPTA_OK, fpta_column_set_validate(&def));
  EXPECT_EQ(FPTA_EINVAL, fpta_column_set_destroy(&def));
}

TEST(Schema, NonUniqPrimary_with_Secondary) {
  fpta_column_set def;
  fpta_column_set_init(&def);
  EXPECT_NE(FPTA_SUCCESS, fpta_column_set_validate(&def));

  //------------------------------------------------------------------------

  EXPECT_EQ(FPTA_OK,
            fpta_column_describe("_id", fptu_uint64,
                                 fpta_secondary_unique_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "_last_changed", fptu_datetime,
                         fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_OK, fpta_column_describe(
                         "port", fptu_int64,
                         fpta_secondary_withdups_ordered_obverse, &def));
  EXPECT_EQ(FPTA_EFLAG,
            fpta_column_describe("host", fptu_cstr,
                                 fpta_primary_withdups_ordered_obverse, &def));
  EXPECT_NE(FPTA_SUCCESS, fpta_column_set_validate(&def));

  //------------------------------------------------------------------------

  EXPECT_EQ(FPTA_OK, fpta_column_set_destroy(&def));
  EXPECT_NE(FPTA_OK, fpta_column_set_validate(&def));
  EXPECT_EQ(FPTA_EINVAL, fpta_column_set_destroy(&def));
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
