#ifndef NANODBC_TEST_BASIC_TEST_H
#define NANODBC_TEST_BASIC_TEST_H

#include "nanodbc.h"
#include <boost/config.hpp>
#include <boost/locale/encoding_utf.hpp>
#include <boost/mpl/list.hpp>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4244) // conversion from 'T1' to 'T2' possible loss of data
#endif
#include <boost/test/unit_test.hpp>

#ifdef NANODBC_USE_UNICODE
    #define NANODBC_TEXT(s) L ## s
#else
    #define NANODBC_TEXT(s) s
#endif

#ifdef _WIN32
// needs to be included above sql.h for windows
#define NOMINMAX
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>

struct basic_test
{
    typedef boost::mpl::list<
        nanodbc::string_type::value_type
        , short
        , unsigned short
        , int32_t
        , uint32_t
        , int64_t
        , uint64_t
        , float
        , double
        > integral_test_types;

    basic_test()
    : connection_string_(get_connection_string_from_env())
    {
    }

    basic_test(const nanodbc::string_type& connection_string)
    : connection_string_(connection_string)
    {
    }

    // Utilities

    nanodbc::string_type connection_string_;

    nanodbc::connection connect()
    {
        return nanodbc::connection(connection_string_);
    }

    static void check_rows_equal(nanodbc::result results, int rows)
    {
        BOOST_CHECK(results.next());
        BOOST_CHECK_EQUAL(results.get<int>(0), rows);
    }

    nanodbc::string_type get_connection_string_from_env() const
    {
        const char* env_name = "NANODBC_TEST_CONNSTR";
        char* env_value = nullptr;
        std::string connection_string;
#ifdef _MSC_VER
        std::size_t env_len(0);
        errno_t err = _dupenv_s(&env_value, &env_len, env_name);
        if(!err && env_value)
        {
            connection_string = env_value;
            std::free(env_value);
        }
#else
        env_value = std::getenv(env_name);
        if(!env_value) return nanodbc::string_type();
        connection_string = env_value;
#endif

#ifdef NANODBC_USE_UNICODE
        using boost::locale::conv::utf_to_utf;
        return utf_to_utf<wchar_t>(connection_string.c_str(),
            connection_string.c_str() + connection_string.size());
#else
        return connection_string;
#endif
    }

    void drop_table(nanodbc::connection& connection, const nanodbc::string_type& name) const
    {
        execute(connection, NANODBC_TEXT("DROP TABLE IF EXISTS ") + name + NANODBC_TEXT(";"));
    }

    // Test Cases

    void catalog_columns_test()
    {
        nanodbc::connection connection = connect();
        BOOST_CHECK(connection.connected());
        nanodbc::catalog catalog(connection);

        // TODO: Tested against SQL Server only and it is failing against PostgreSQL
        //       We are going to need postgresql_test.cpp, as we do with sqlite_test.cpp.
        //       It also seems to fail for MySQL as well. -- lexicalunit

        // Check we can iterate over any columns
        {
            nanodbc::catalog::columns columns = catalog.find_columns();
            long count = 0;
            while(columns.next())
            {
                // These values must not be NULL (returned as empty string)
                BOOST_CHECK(!columns.column_name().empty());
                count++;
            }
            BOOST_CHECK_GT(count, 0);
        }

        // Find a table with known name and verify its known columns
        {
            nanodbc::string_type const table_name(NANODBC_TEXT("catalog_columns_test"));
            drop_table(connection, table_name);
            execute(connection, NANODBC_TEXT("create table ") + table_name + NANODBC_TEXT("(")
                + NANODBC_TEXT("c0 int PRIMARY KEY,")
                + NANODBC_TEXT("c1 smallint NOT NULL,")
                + NANODBC_TEXT("c2 float NULL,")
                + NANODBC_TEXT("c3 decimal(9, 3),")
                + NANODBC_TEXT("c4 date,") // seems more portable than datetime (SQL Server), timestamp (PostgreSQL, MySQL)
                + NANODBC_TEXT("c5 varchar(60) DEFAULT \'sample value\',")
                + NANODBC_TEXT("c6 nvarchar(120),")
                + NANODBC_TEXT("c7 text,")
                + NANODBC_TEXT("c8 varbinary,")
                + NANODBC_TEXT("c9 varbinary(255)")
                + NANODBC_TEXT(");")
                );

            // Check only SQL/ODBC standard properties, skip those which are driver-specific.
            nanodbc::catalog::columns columns = catalog.find_columns(NANODBC_TEXT("%"), table_name);

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c0"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(), SQL_INTEGER);
            BOOST_CHECK_EQUAL(columns.column_size(), 10);
            BOOST_CHECK_EQUAL(columns.decimal_digits(), 0);
            BOOST_CHECK_EQUAL(columns.nullable(), SQL_NO_NULLS);
            BOOST_CHECK(columns.table_name() == table_name); // assume common for the whole result set, check once
            BOOST_CHECK(!columns.type_name().empty()); // data source dependant name, check once
            BOOST_CHECK(columns.is_nullable() == NANODBC_TEXT("NO"));

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c1"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_SMALLINT);
            BOOST_CHECK_EQUAL(columns.column_size(), 5);
            BOOST_CHECK_EQUAL(columns.decimal_digits(), 0);
            BOOST_CHECK_EQUAL(columns.nullable(), SQL_NO_NULLS);
            BOOST_CHECK(columns.is_nullable() == NANODBC_TEXT("NO"));

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c2"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_FLOAT);
            BOOST_CHECK_EQUAL(columns.column_size(),  53); // total number of bits allowed
            BOOST_CHECK_EQUAL(columns.nullable(), SQL_NULLABLE);
            BOOST_CHECK(columns.is_nullable() == NANODBC_TEXT("YES"));

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c3"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_DECIMAL);
            BOOST_CHECK_EQUAL(columns.column_size(),  9);
            BOOST_CHECK_EQUAL(columns.decimal_digits(),  3);
            BOOST_CHECK_EQUAL(columns.nullable(), SQL_NULLABLE);
            BOOST_CHECK(columns.is_nullable() == NANODBC_TEXT("YES"));

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c4"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_DATE);
            BOOST_CHECK_EQUAL(columns.column_size(),  23); // total number of characters required to display the value when it is converted to characters

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c5"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_VARCHAR);
            BOOST_CHECK_EQUAL(columns.column_size(),  60);
            BOOST_CHECK(columns.column_default() == NANODBC_TEXT("\'sample value\'"));

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c6"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_WVARCHAR);
            BOOST_CHECK_EQUAL(columns.column_size(),  120);

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c7"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_LONGVARCHAR);
            BOOST_CHECK_EQUAL(columns.column_size(),  2147483647); // TODO: confirm "text" max length

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c8"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_VARBINARY);
            BOOST_CHECK_EQUAL(columns.column_size(),  1); // if n is not specified in [var]binary(n), the default length is 1

            BOOST_CHECK(columns.next());
            BOOST_CHECK(columns.column_name() == NANODBC_TEXT("c9"));
            BOOST_CHECK_EQUAL(columns.sql_data_type(),  SQL_VARBINARY);
            BOOST_CHECK_EQUAL(columns.column_size(),  255);

            // expect no more records
            BOOST_CHECK(!columns.next());
        }
    }

    void catalog_primary_keys_test()
    {
        nanodbc::connection connection = connect();
        BOOST_CHECK(connection.connected());
        nanodbc::catalog catalog(connection);

        // TODO: Tested against SQL Server only and it is failing against PostgreSQL
        //       We are going to need postgresql_test.cpp, as we do with sqlite_test.cpp.

        // Find a single-column primary key for table with known name
        {
            nanodbc::string_type const table_name(NANODBC_TEXT("catalog_primary_keys_simple_test"));
            drop_table(connection, table_name);
            execute(connection, NANODBC_TEXT("create table ") + table_name
                + NANODBC_TEXT("(i int NOT NULL, CONSTRAINT pk_simple_test PRIMARY KEY (i));"));

            nanodbc::catalog::primary_keys keys =  catalog.find_primary_keys(table_name);
            BOOST_CHECK(keys.next());
            BOOST_CHECK(keys.table_name() == table_name);
            BOOST_CHECK(keys.column_name() == NANODBC_TEXT("i"));
            BOOST_CHECK_EQUAL(keys.column_number(), 1);
            BOOST_CHECK(keys.primary_key_name() == NANODBC_TEXT("pk_simple_test"));
            // expect no more records
            BOOST_CHECK(!keys.next());
        }

        // Find a multi-column primary key for table with known name
        {
            nanodbc::string_type const table_name(NANODBC_TEXT("catalog_primary_keys_composite_test"));
            drop_table(connection, table_name);
            execute(connection, NANODBC_TEXT("create table ") + table_name
                + NANODBC_TEXT("(a int, b smallint, CONSTRAINT pk_composite_test PRIMARY KEY(a, b));"));

            nanodbc::catalog::primary_keys keys =  catalog.find_primary_keys(table_name);
            BOOST_CHECK(keys.next());
            BOOST_CHECK(keys.table_name() == table_name);
            BOOST_CHECK(keys.column_name() == NANODBC_TEXT("a"));
            BOOST_CHECK_EQUAL(keys.column_number(), 1);
            BOOST_CHECK(keys.primary_key_name() == NANODBC_TEXT("pk_composite_test"));

            BOOST_CHECK(keys.next());
            BOOST_CHECK(keys.table_name() == table_name);
            BOOST_CHECK(keys.column_name() == NANODBC_TEXT("b"));
            BOOST_CHECK_EQUAL(keys.column_number(), 2);
            BOOST_CHECK(keys.primary_key_name() == NANODBC_TEXT("pk_composite_test"));

            // expect no more records
            BOOST_CHECK(!keys.next());
        }
    }
    void catalog_tables_test()
    {
        nanodbc::connection connection = connect();
        BOOST_CHECK(connection.connected());
        nanodbc::catalog catalog(connection);

        // TODO: Tested against SQL Server only and it is failing against PostgreSQL
        //       We are going to need postgresql_test.cpp, as we do with sqlite_test.cpp.

        // Check we can iterate over any tables
        {
            nanodbc::catalog::tables tables = catalog.find_tables();
            long count = 0;
            while(tables.next())
            {
                // These two values must not be NULL (returned as empty string)
                BOOST_CHECK(!tables.table_name().empty());
                BOOST_CHECK(!tables.table_type().empty());
                count++;
            }
            BOOST_CHECK_GT(count, 0);
        }

        // Check if there are any tables (with catalog restriction)
        {
            nanodbc::string_type empty_name; // a placeholder, makes no restriction on the look-up
            nanodbc::catalog::tables tables = catalog.find_tables(empty_name, NANODBC_TEXT("TABLE"), empty_name, empty_name);
            long count = 0;
            while(tables.next())
            {
                // These two values must not be NULL (returned as empty string)
                BOOST_CHECK(!tables.table_name().empty());
                BOOST_CHECK(!tables.table_type().empty());
                count++;
            }
            BOOST_CHECK_GT(count, 0);
        }

        // Find a table with known name
        {
            nanodbc::string_type const table_name(NANODBC_TEXT("catalog_tables_test"));
            drop_table(connection, table_name);
            execute(connection, NANODBC_TEXT("create table ") + table_name + NANODBC_TEXT("(a int);"));

            // Use brute-force look-up
            {
                nanodbc::catalog::tables tables = catalog.find_tables();
                bool found = false;
                while(tables.next())
                {
                    if(table_name == tables.table_name())
                    {
                        BOOST_CHECK(tables.table_type() == NANODBC_TEXT("TABLE"));
                        found = true;
                        break;
                    }
                }
                BOOST_CHECK(found);
            }

            // Use SQLTables pattern search capabilities
            {
                nanodbc::catalog::tables tables = catalog.find_tables(table_name);
                // expect single record with the wanted table
                BOOST_CHECK(tables.next());
                BOOST_CHECK(tables.table_name() == table_name);
                BOOST_CHECK(tables.table_type() == NANODBC_TEXT("TABLE"));
                // expect no more records
                BOOST_CHECK(!tables.next());
            }
        }

        // Find a VIEW with known name
        {
            nanodbc::string_type const view_name(NANODBC_TEXT("TABLE_PRIVILEGES"));

            // Use SQLTables pattern search by name only (in any schema)
            {
                nanodbc::catalog::tables tables = catalog.find_tables(view_name, NANODBC_TEXT("VIEW"));
                // expect single record with the wanted table
                BOOST_CHECK(tables.next());
                BOOST_CHECK(tables.table_name() == view_name);
                BOOST_CHECK(tables.table_type() == NANODBC_TEXT("VIEW"));
                // expect no more records
                BOOST_CHECK(!tables.next());
            }

            // Use SQLTables pattern search by name inside given schema
            {
                nanodbc::string_type const schema_name(NANODBC_TEXT("INFORMATION_SCHEMA"));
                nanodbc::catalog::tables tables = catalog.find_tables(view_name, NANODBC_TEXT("VIEW"), schema_name);
                // expect single record with the wanted table
                BOOST_CHECK(tables.next());
                BOOST_CHECK(tables.table_schema() == schema_name);
                BOOST_CHECK(tables.table_name() == view_name);
                BOOST_CHECK(tables.table_type() == NANODBC_TEXT("VIEW"));
                // expect no more records
                BOOST_CHECK(!tables.next());
            }
        }
    }

    void decimal_conversion_test()
    {
        nanodbc::connection connection = connect();
        nanodbc::result results;
        drop_table(connection, NANODBC_TEXT("decimal_conversion_test"));
        execute(connection, NANODBC_TEXT("create table decimal_conversion_test (d decimal(9, 3));"));
        execute(connection, NANODBC_TEXT("insert into decimal_conversion_test values (12345.987);"));
        execute(connection, NANODBC_TEXT("insert into decimal_conversion_test values (5.600);"));
        execute(connection, NANODBC_TEXT("insert into decimal_conversion_test values (1.000);"));
        execute(connection, NANODBC_TEXT("insert into decimal_conversion_test values (-1.333);"));
        results = execute(connection, NANODBC_TEXT("select * from decimal_conversion_test order by 1 desc;"));

        BOOST_CHECK(results.next());
        BOOST_CHECK(results.get<nanodbc::string_type>(0) == NANODBC_TEXT("12345.987"));

        BOOST_CHECK(results.next());
        BOOST_CHECK(results.get<nanodbc::string_type>(0) == NANODBC_TEXT("5.600"));

        BOOST_CHECK(results.next());
        BOOST_CHECK(results.get<nanodbc::string_type>(0) == NANODBC_TEXT("1.000"));

        BOOST_CHECK(results.next());
        BOOST_CHECK(results.get<nanodbc::string_type>(0) == NANODBC_TEXT("-1.333"));
    }

    void exception_test()
    {
        nanodbc::connection connection = connect();
        nanodbc::result results;

        BOOST_CHECK_THROW(execute(connection, NANODBC_TEXT("THIS IS NOT VALID SQL!")), nanodbc::database_error);

        drop_table(connection, NANODBC_TEXT("exception_test"));
        execute(connection, NANODBC_TEXT("create table exception_test (i int);"));
        execute(connection, NANODBC_TEXT("insert into exception_test values (-10);"));
        execute(connection, NANODBC_TEXT("insert into exception_test values (null);"));

        results = execute(connection, NANODBC_TEXT("select * from exception_test where i = -10;"));

        BOOST_CHECK(results.next());
        BOOST_CHECK_THROW(results.get<nanodbc::date>(0), nanodbc::type_incompatible_error);
        BOOST_CHECK_THROW(results.get<nanodbc::timestamp>(0), nanodbc::type_incompatible_error);

        results = execute(connection, NANODBC_TEXT("select * from exception_test where i is null;"));

        BOOST_CHECK(results.next());
        BOOST_CHECK_THROW(results.get<int>(0), nanodbc::null_access_error);
        BOOST_CHECK_THROW(results.get<int>(42), nanodbc::index_range_error);

        nanodbc::statement statement(connection);
        BOOST_CHECK(statement.open() && statement.connected());
        statement.close();
        BOOST_CHECK_THROW(statement.prepare(NANODBC_TEXT("select * from exception_test;")), nanodbc::programming_error);
    }

    void execute_multiple_transaction_test()
    {
        nanodbc::connection connection = connect();
        nanodbc::statement statement;
        nanodbc::result results;

        statement.prepare(connection, NANODBC_TEXT("select 42;"));

        {
            nanodbc::transaction transaction(connection);
            results = statement.execute();
            results.next();
            BOOST_CHECK_EQUAL(results.get<int>(0), 42);
        }

        results = statement.execute();
        results.next();
        BOOST_CHECK_EQUAL(results.get<int>(0), 42);
    }

    void execute_multiple_test()
    {
        nanodbc::connection connection = connect();
        nanodbc::statement statement(connection);
        nanodbc::prepare(statement, NANODBC_TEXT("select 42;"));

        nanodbc::result results = statement.execute();
        results.next();

        results = statement.execute();
        results.next();
        BOOST_CHECK_EQUAL(results.get<int>(0), 42);

        results = statement.execute();
        results.next();
        BOOST_CHECK_EQUAL(results.get<int>(0), 42);
    }

    template<class T>
    void integral_test_template()
    {
        nanodbc::connection connection = connect();

        drop_table(connection, NANODBC_TEXT("integral_test"));
        execute(connection, NANODBC_TEXT("create table integral_test (i int, f float, d double precision);"));

        nanodbc::statement statement(connection);
        prepare(statement, NANODBC_TEXT("insert into integral_test (i, f, d) values (?, ?, ?);"));

        srand(0);
        const int32_t i = rand() % 5000;
        const float f = rand() / (rand() + 1.0);
        const float d = - rand() / (rand() + 1.0);

        short p = 0;
        statement.bind(p++, &i);
        statement.bind(p++, &f);
        statement.bind(p++, &d);

        BOOST_CHECK(statement.connected());
        execute(statement);

        nanodbc::result results = execute(connection, NANODBC_TEXT("select * from integral_test;"));
        BOOST_CHECK(results.next());

        T ref;
        p = 0;
        results.get_ref(p, ref);
        BOOST_CHECK_EQUAL(ref, static_cast<T>(i));
        BOOST_CHECK_EQUAL(results.get<T>(p++), static_cast<T>(i));
        results.get_ref(p, ref);
        BOOST_CHECK_CLOSE(static_cast<float>(ref), static_cast<T>(f), 1e-3);
        BOOST_CHECK_CLOSE(static_cast<float>(results.get<T>(p++)), static_cast<T>(f), 1e-3);
        results.get_ref(p, ref);
        BOOST_CHECK_CLOSE(static_cast<double>(ref), static_cast<T>(d), 1e-3);
        BOOST_CHECK_CLOSE(static_cast<double>(results.get<T>(p++)), static_cast<T>(d), 1e-3);
    }

    #ifndef BOOST_NO_RVALUE_REFERENCES
    void move_test()
    {
        nanodbc::connection orig_connection = connect();
        drop_table(orig_connection, NANODBC_TEXT("move_test"));
        execute(orig_connection, NANODBC_TEXT("create table move_test (i int);"));
        execute(orig_connection, NANODBC_TEXT("insert into move_test values (10);"));

        nanodbc::connection new_connection = std::move(orig_connection);
        execute(new_connection, NANODBC_TEXT("insert into move_test values (30);"));
        execute(new_connection, NANODBC_TEXT("insert into move_test values (20);"));

        nanodbc::result orig_results = execute(new_connection,
            NANODBC_TEXT("select i from move_test order by i desc;")
        );
        BOOST_CHECK(orig_results.next());
        BOOST_CHECK_EQUAL(orig_results.get<int>(0), 30);
        BOOST_CHECK(orig_results.next());
        BOOST_CHECK_EQUAL(orig_results.get<int>(0), 20);

        nanodbc::result new_results = std::move(orig_results);
        BOOST_CHECK(new_results.next());
        BOOST_CHECK_EQUAL(new_results.get<int>(0), 10);
    }
    #endif // BOOST_NO_RVALUE_REFERENCES

    void null_test()
    {
        nanodbc::connection connection = connect();
        BOOST_CHECK(connection.connected());

        drop_table(connection, NANODBC_TEXT("null_test"));
        execute(connection, NANODBC_TEXT("create table null_test (a int, b varchar(10));"));

        nanodbc::statement statement(connection);

        prepare(statement, NANODBC_TEXT("insert into null_test (a, b) values (?, ?);"));
        statement.bind_null(0);
        statement.bind_null(1);
        execute(statement);

        prepare(statement, NANODBC_TEXT("insert into null_test (a, b) values (?, ?);"));
        statement.bind_null(0, 2);
        statement.bind_null(1, 2);
        execute(statement, 2);

        nanodbc::result results = execute(connection, NANODBC_TEXT("select a, b from null_test order by a;"));

        BOOST_CHECK(results.next());
        BOOST_CHECK(results.is_null(0));
        BOOST_CHECK(results.is_null(1));

        BOOST_CHECK(results.next());
        BOOST_CHECK(results.is_null(0));
        BOOST_CHECK(results.is_null(1));

        BOOST_CHECK(results.next());
        BOOST_CHECK(results.is_null(0));
        BOOST_CHECK(results.is_null(1));

        BOOST_CHECK(!results.next());
    }

    void simple_test()
    {
        nanodbc::connection connection = connect();
        BOOST_CHECK(connection.connected());
        BOOST_CHECK(connection.native_dbc_handle());
        BOOST_CHECK(connection.native_env_handle());
        BOOST_CHECK_EQUAL(connection.transactions(), std::size_t(0));

        drop_table(connection, NANODBC_TEXT("simple_test"));
        execute(connection, NANODBC_TEXT("create table simple_test (a int, b varchar(10));"));
        execute(connection, NANODBC_TEXT("insert into simple_test values (1, 'one');"));
        execute(connection, NANODBC_TEXT("insert into simple_test values (2, 'two');"));
        execute(connection, NANODBC_TEXT("insert into simple_test values (3, 'tri');"));
        execute(connection, NANODBC_TEXT("insert into simple_test (b) values ('z');"));

        {
            nanodbc::result results = execute(connection, NANODBC_TEXT("select a, b from simple_test order by a;"));
            BOOST_CHECK((bool)results);
            BOOST_CHECK_EQUAL(results.rows(), 0);
            BOOST_CHECK_EQUAL(results.columns(), 2);

            // From MSDN on SQLRowCount:
            // Row count is either the number of rows affected by the request
            // or -1 if the number of affected rows is not available.
            // For other statements and functions, the driver may define the value returned (...)
            // some data sources may be able to return the number of rows returned by a SELECT statement.
            BOOST_CHECK(results.affected_rows() == 4 || results.affected_rows() == 0 || results.affected_rows() == -1);

            BOOST_CHECK_EQUAL(results.rowset_size(), 1);
            BOOST_CHECK(results.column_name(0) == NANODBC_TEXT("a"));
            BOOST_CHECK(results.column_name(1) == NANODBC_TEXT("b"));

            BOOST_CHECK(results.next());
            BOOST_CHECK_EQUAL(results.rows(), 1);
            BOOST_CHECK(results.is_null(0));
            BOOST_CHECK(results.is_null(NANODBC_TEXT("a")));
            BOOST_CHECK_EQUAL(results.get<int>(0, -1), -1);
            BOOST_CHECK_EQUAL(results.get<int>(NANODBC_TEXT("a"), -1), -1);
            BOOST_CHECK(results.get<nanodbc::string_type>(0, NANODBC_TEXT("null")) == NANODBC_TEXT("null"));
            BOOST_CHECK(results.get<nanodbc::string_type>(NANODBC_TEXT("a"), NANODBC_TEXT("null")) == NANODBC_TEXT("null"));
            BOOST_CHECK(results.get<nanodbc::string_type>(1) == NANODBC_TEXT("z"));
            BOOST_CHECK(results.get<nanodbc::string_type>(NANODBC_TEXT("b")) == NANODBC_TEXT("z"));

            int ref_int;
            results.get_ref(0, -1, ref_int);
            BOOST_CHECK_EQUAL(ref_int, -1);
            results.get_ref(NANODBC_TEXT("a"), -2, ref_int);
            BOOST_CHECK_EQUAL(ref_int, -2);

            nanodbc::string_type ref_str;
            results.get_ref<nanodbc::string_type>(0, NANODBC_TEXT("null"), ref_str);
            BOOST_CHECK(ref_str == NANODBC_TEXT("null"));
            results.get_ref<nanodbc::string_type>(NANODBC_TEXT("a"), NANODBC_TEXT("null2"), ref_str);
            BOOST_CHECK(ref_str == NANODBC_TEXT("null2"));

            BOOST_CHECK(results.next());
            BOOST_CHECK_EQUAL(results.get<int>(0), 1);
            BOOST_CHECK_EQUAL(results.get<int>(NANODBC_TEXT("a")), 1);
            BOOST_CHECK(results.get<nanodbc::string_type>(1) == NANODBC_TEXT("one"));
            BOOST_CHECK(results.get<nanodbc::string_type>(NANODBC_TEXT("b")) == NANODBC_TEXT("one"));

            nanodbc::result results_copy = results;

            BOOST_CHECK(results_copy.next());
            BOOST_CHECK_EQUAL(results_copy.get<int>(0, -1), 2);
            BOOST_CHECK_EQUAL(results_copy.get<int>(NANODBC_TEXT("a"), -1), 2);
            BOOST_CHECK(results_copy.get<nanodbc::string_type>(1) == NANODBC_TEXT("two"));
            BOOST_CHECK(results_copy.get<nanodbc::string_type>(NANODBC_TEXT("b")) == NANODBC_TEXT("two"));

            // FIXME: not supported by the default SQL_CURSOR_FORWARD_ONLY
            // and will require SQL_ATTR_CURSOR_TYPE set to SQL_CURSOR_STATIC at least.
            //BOOST_CHECK(results.position());

            nanodbc::result().swap(results_copy);

            BOOST_CHECK(results.next());
            BOOST_CHECK(results.get<nanodbc::string_type>(0) == NANODBC_TEXT("3"));
            BOOST_CHECK(results.get<nanodbc::string_type>(NANODBC_TEXT("a")) == NANODBC_TEXT("3"));
            BOOST_CHECK(results.get<nanodbc::string_type>(1) == NANODBC_TEXT("tri"));
            BOOST_CHECK(results.get<nanodbc::string_type>(NANODBC_TEXT("b")) == NANODBC_TEXT("tri"));

            BOOST_CHECK(!results.next());
            BOOST_CHECK(results.end());
        }

        nanodbc::connection connection_copy(connection);

        connection.disconnect();
        BOOST_CHECK(!connection.connected());
        BOOST_CHECK(!connection_copy.connected());
    }

    void string_test()
    {
        nanodbc::connection connection = connect();
        BOOST_CHECK(connection.connected());
        BOOST_CHECK(connection.native_dbc_handle());
        BOOST_CHECK(connection.native_env_handle());
        BOOST_CHECK_EQUAL(connection.transactions(), std::size_t(0));

        const nanodbc::string_type name = NANODBC_TEXT("Fred");

        drop_table(connection, NANODBC_TEXT("string_test"));
        execute(connection, NANODBC_TEXT("create table string_test (s varchar(10));"));

        nanodbc::statement query(connection);
        prepare(query, NANODBC_TEXT("insert into string_test(s) values(?)"));
        query.bind(0, name.c_str());
        nanodbc::execute(query);

        nanodbc::result results = execute(connection, NANODBC_TEXT("select s from string_test;"));
        BOOST_CHECK(results.next());
        BOOST_CHECK(results.get<nanodbc::string_type>(0) == NANODBC_TEXT("Fred"));

        nanodbc::string_type ref;
        results.get_ref(0, ref);
        BOOST_CHECK(ref == name);
    }

    void transaction_test()
    {
        nanodbc::connection connection = connect();
        BOOST_CHECK(connection.connected());

        drop_table(connection, NANODBC_TEXT("transaction_test"));
        execute(connection, NANODBC_TEXT("create table transaction_test (i int);"));

        nanodbc::statement statement(connection);
        prepare(statement, NANODBC_TEXT("insert into transaction_test (i) values (?);"));

        static const int elements = 10;
        int data[elements] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        statement.bind(0, data, elements);
        execute(statement, elements);

        static const nanodbc::string_type::value_type* query = NANODBC_TEXT("select count(1) from transaction_test;");

        check_rows_equal(execute(connection, query), 10);

        {
            nanodbc::transaction transaction(connection);
            execute(connection, NANODBC_TEXT("delete from transaction_test;"));
            check_rows_equal(execute(connection, query), 0);
            // ~transaction() calls rollback()
        }

        check_rows_equal(execute(connection, query), 10);

        {
            nanodbc::transaction transaction(connection);
            execute(connection, NANODBC_TEXT("delete from transaction_test;"));
            check_rows_equal(execute(connection, query), 0);
            transaction.rollback();
        }

        check_rows_equal(execute(connection, query), 10);

        {
            nanodbc::transaction transaction(connection);
            execute(connection, NANODBC_TEXT("delete from transaction_test;"));
            check_rows_equal(execute(connection, query), 0);
            transaction.commit();
        }

        check_rows_equal(execute(connection, query), 0);
    }

    void while_not_end_iteration_test()
    {
        nanodbc::connection connection = connect();
        drop_table(connection, NANODBC_TEXT("while_not_end_iteration_test"));
        execute(connection, NANODBC_TEXT("create table while_not_end_iteration_test (i int);"));
        execute(connection, NANODBC_TEXT("insert into while_not_end_iteration_test values (1);"));
        execute(connection, NANODBC_TEXT("insert into while_not_end_iteration_test values (2);"));
        execute(connection, NANODBC_TEXT("insert into while_not_end_iteration_test values (3);"));
        nanodbc::result results = execute(connection, NANODBC_TEXT("select * from while_not_end_iteration_test order by 1 desc;"));
        int i = 3;
        while(!results.end())
        {
            results.next();
            BOOST_CHECK_EQUAL(results.get<int>(0), i--);
        }
    }

    void while_next_iteration_test()
    {
        nanodbc::connection connection = connect();
        drop_table(connection, NANODBC_TEXT("while_next_iteration_test"));
        execute(connection, NANODBC_TEXT("create table while_next_iteration_test (i int);"));
        execute(connection, NANODBC_TEXT("insert into while_next_iteration_test values (1);"));
        execute(connection, NANODBC_TEXT("insert into while_next_iteration_test values (2);"));
        execute(connection, NANODBC_TEXT("insert into while_next_iteration_test values (3);"));
        nanodbc::result results = execute(connection, NANODBC_TEXT("select * from while_next_iteration_test order by 1 desc;"));
        int i = 3;
        while(results.next())
        {
            BOOST_CHECK_EQUAL(results.get<int>(0), i--);
        }
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // NANODBC_TEST_BASIC_TEST_H
