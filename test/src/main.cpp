#include <functional>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <vector>

#include <utilcpp/disable_copy.h>
#include <utilcpp/declarations.h>

#include <datamappercpp/sql/Repository.h>
#include <datamappercpp/sql/db.h>
#include <datamappercpp/sql/detail/SqlStatementBuilder.h>

#include <testcpp/testcpp.h>
#include <testcpp/StdOutView.h>


/* Include <datamappercpp/sql/util/trace.h>
 * and call dm::sql::TraceSqlToStderr();
 * if you want to trace SQL statements as SQLite sees them.
 */

struct Person
{
    typedef std::vector<Person> list;
    typedef std::vector<boost::shared_ptr<Person>> listPtr;

    __int64 id;
    std::string name;
    std::wstring wname;
    __int64 age;
    double height;

    Person() :
        id(-1), name(), wname(), age(0), height(0.0)
    { }

    Person(__int64 i, const std::string& n, const std::wstring& wn, __int64 a, double h) :
        id(i), name(n), wname(wn), age(a), height(h)
    { }

    bool operator==(const Person& rhs) const
    {
        return id == rhs.id && name == rhs.name && wname == rhs.wname
            && age == rhs.age && height == rhs.height;
    }

    operator std::wstring() const
    {
        std::wstring nameWstring;
        #pragma warning(disable:4996)//We are perfectly aware the mbstowcs is not "safe" operation as the destination size is not given - fortunately our destination has dynamic size.
        mbstowcs(&nameWstring[0], &name[0], name.length());
        #pragma warning(default:4996)
        std::wostringstream out;
        out << "Person: {"
            << id << ","
            << nameWstring << ","
            << age << ","
            << height << "}";
        return out.str();
    }

};

void print_person(const Person& p)
{ std::wcout << static_cast<std::wstring>(p) << std::endl; }

class PersonMapping
{
public:
    static std::string getLabel()
    { return "person"; }

    template <class Visitor>
    static void accept(Visitor& v, Person& p)
    {
        // Note that field order is important
        v.visitField(dm::Field<std::string>("name", "UNIQUE NOT NULL"), p.name);
        v.visitField(dm::Field<std::wstring>("wname"), p.wname);
        v.visitField(dm::Field<__int64>("age"), p.age);
        v.visitField(dm::Field<double>("height"), p.height);
    }

    static std::string customCreateStatements()
    {
        return "CREATE INDEX IF NOT EXISTS person_name_idx "
            "ON person (name)";
    }
};

class PersonRepository : public dm::sql::Repository<Person, PersonMapping>
{ };

class TestDataMapperCpp : public Test::Suite
{
public:
    typedef void (TestDataMapperCpp::*TestMethod)();

    TestDataMapperCpp()
    {
        dm::sql::ConnectDatabase("test.sqlite");
        dm::sql::ExecuteStatement(std::string("DROP TABLE IF EXISTS ")
            + PersonMapping::getLabel());
    }

    ~TestDataMapperCpp()
    {
        dm::sql::ExecuteStatement(std::string("DROP TABLE IF EXISTS ")
                + PersonMapping::getLabel());
    }

    void test()
    {
        testSqlStatementBuilding();
        testObjectSaving();
        testSingleObjectLoading();
        testMultipleObjectLoading();
        testObjectDeletion();
        // TODO: test transactions
    }

    void testSqlStatementBuilding()
    {
        typedef dm::sql::SqlStatementBuilder<Person, PersonMapping> PersonSql;

        Test::assertEqual<std::string>(
            "Create table statement is correct",
            PersonSql::CreateTableStatement(),
            "CREATE TABLE IF NOT EXISTS person"
            "(id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT UNIQUE NOT NULL,"
            "wname TEXT,"
            "age INT,"
            "height REAL);"
            "CREATE INDEX IF NOT EXISTS person_name_idx ON person (name)");

        Test::assertEqual<std::string>(
                "Insert statement is correct",
                PersonSql::InsertStatement(),
                "INSERT INTO person (name,wname,age,height) VALUES (?,?,?,?)");

        Test::assertEqual<std::string>(
                "Update statement is correct",
                PersonSql::UpdateStatement(),
                "UPDATE person SET name=?,wname=?,age=?,height=? WHERE id=?");

        Test::assertEqual<std::string>(
                "Select by ID statement is correct",
                PersonSql::SelectByIdStatement(),
                "SELECT * FROM person WHERE id=?");

        Test::assertEqual<std::string>(
                "Select by field statement is correct",
                PersonSql::SelectByFieldStatement("age"),
                "SELECT * FROM person WHERE age=?");
    }

    void testObjectSaving()
    {
        PersonRepository::CreateTable();

        Person p(-1, "Ervin", L"ŽŸ®", 8446744073709551615, 1.80);
        PersonRepository::Save(p);
        Test::assertTrue(
                "Object ID is correctly set during saving",
                p.id == 1);

        Person::list ps;

        ps.push_back(Person(-1, "Marvin", L"¥¥¥¥", 24, 1.65));
        ps.push_back(Person(-1, "Steve", L"Ž¥¥", 32, 2.10));

        PersonRepository::Save(ps);
        Test::assertTrue(
                "Multiple objects are correctly saved",
                ps[0].id == 2 && ps[1].id == 3);
    }

    void testSingleObjectLoading()
    {
        Person p = PersonRepository::Get(1);
        Test::assertTrue("Get objects by ID works",
			p.name == "Ervin" && p.wname == L"ŽŸ®" && p.age == 8446744073709551615 && p.height == 1.80);

        p = PersonRepository::GetByField("name", "Marvin");
        Test::assertTrue("Get objects by field works",
                p.name == "Marvin" &&  p.wname == L"¥¥¥¥" && p.age == 24 && p.height == 1.65);

        p = PersonRepository::GetByQuery("SELECT * FROM person "
                "WHERE name LIKE '%eve'");
        Test::assertTrue("Get objects by query works",
                p.name == "Steve" &&  p.wname == L"Ž¥¥" && p.age == 32 && p.height == 2.10);

        dm::sql::Statement statement = dm::sql::PrepareStatement(
                "SELECT * FROM person WHERE name LIKE '%vin'");
        p = PersonRepository::GetByQuery(statement, true);
        Test::assertTrue(
                "More than one result causes the first one to be returned "
                "if allow_many is true",
                p.name == "Ervin" && p.wname == L"ŽŸ®" && p.age == 8446744073709551615 && p.height == 1.80);

        statement = dm::sql::PrepareStatement(
                "SELECT * FROM person WHERE wname LIKE ? AND age = ?");
        *statement << L"%Ÿ®" << 8446744073709551615;
        p = PersonRepository::GetByQuery(statement);
        Test::assertTrue(
                "Statement operator << works for wstring and __int64",
                p.name == "Ervin" && p.wname == L"ŽŸ®" && p.age == 8446744073709551615 && p.height == 1.80);

        Test::assertThrows<TestDataMapperCpp, TestMethod,
                           dm::sql::NotOneError>(
                "More than one result causes NotOneError exception "
                "if allow_many is false",
                *this, &TestDataMapperCpp::ifManyResults_ThenThrowsNotOneError);

        Test::assertThrows<TestDataMapperCpp, TestMethod,
                           dm::sql::DoesNotExistError>(
                "No results for given criteria causes "
                "DoesNotExistError exception",
                *this,
                &TestDataMapperCpp::ifDoesNotExist_ThenThrowsDoesNotExistError);
    }

    void testMultipleObjectLoading()
    {
        Person::list expected;
        expected.push_back(Person(1, "Ervin", L"ŽŸ®", 8446744073709551615, 1.80));
        expected.push_back(Person(2, "Marvin", L"¥¥¥¥", 24, 1.65));
        expected.push_back(Person(3, "Steve", L"Ž¥¥", 32, 2.10));

        Person::list ps = PersonRepository::GetAll();

        Test::assertEqual<Person::list>("Get all objects works",
                ps, expected);

        expected[1].age = 32; // Marvin was lying about his age!
        PersonRepository::Save(expected[1]);

        ps = PersonRepository::GetManyByField("age", 32);
        expected.erase(expected.begin()); // remove Ervin, who's still 38

        Test::assertEqual<Person::list>("Get many objects by field works",
                ps, expected);

        Person::listPtr listPtrExpected;
        listPtrExpected.push_back(boost::shared_ptr<Person>(new Person(1, "Ervin", L"ŽŸ®", 8446744073709551615, 1.80)));
        Person::listPtr listPtr = PersonRepository::GetManyByFieldPtr("age", 8446744073709551615);
        Test::assertEqual<Person>("Get many object pointers by field works",
            *listPtr[0], *listPtrExpected[0]);

        expected.clear();
        expected.push_back(Person(1, "Ervin", L"ŽŸ®", 8446744073709551615, 1.80));
        expected.push_back(Person(2, "Marvin", L"¥¥¥¥", 32, 1.65));

        ps = PersonRepository::GetManyByQuery("SELECT * FROM person "
                "WHERE name LIKE '%vin'");
        Test::assertEqual<Person::list>("Get many objects by query works",
                ps, expected);

        dm::sql::Statement statement = dm::sql::PrepareStatement(
                "SELECT * FROM person WHERE name LIKE '%vin'");
        ps = PersonRepository::GetManyByQuery(statement);
        Test::assertEqual<Person::list>("Get many objects by statement works",
                ps, expected);

        ps = PersonRepository::GetManyByField("age", 100);
        expected.clear();

        Test::assertEqual<Person::list>(
                "No results for given criteria returns empty list",
                ps, expected);
    }

    void testObjectUpdating()
    {

    }

    void testObjectDeletion()
    {
        PersonRepository::Delete(1); // Ervin is gone

        Person marvin(2, "Marvin", L"¥¥¥¥", 24, 1.65);
        PersonRepository::Delete(marvin); // Marvin is gone too

        Test::assertTrue(
                "Object ID is invalidated during deletion",
                marvin.id == -1);

        Test::assertThrows<TestDataMapperCpp, TestMethod,
                           dm::sql::DoesNotExistError>(
                "Getting deleted objects causes DoesNotExistError exception",
                *this,
                &TestDataMapperCpp::ifIsDeleted_ThenThrowsDoesNotExistError);

        PersonRepository::DeleteAll();

        Person::list ps = PersonRepository::GetAll();
        Person::list expected;

        Test::assertEqual<Person::list>("Repository is empty after all "
                "objects have been deleted",
                ps, expected);
    }

    void ifDoesNotExist_ThenThrowsDoesNotExistError()
    {
        PersonRepository::Get(42);
    }

    void ifManyResults_ThenThrowsNotOneError()
    {
        PersonRepository::GetByQuery(
                "SELECT * FROM person WHERE name LIKE '%vin'");
    }

    void ifIsDeleted_ThenThrowsDoesNotExistError()
    {
        PersonRepository::Get(1);
    }

    /*
    void testUpdate()
    {
        p.age = 12;
        // would need a mock/fake here that would intercept and assure
        // that update was called instead of insert
        PersonRepository::Save(p);
    }
    */


};

int main()
{
    Test::Controller &c = Test::Controller::instance();
    c.setObserver(Test::observer_transferable_ptr(new Test::ColoredStdOutView));
    c.addTestSuite("datamapper-cpp", Test::Suite::instance<TestDataMapperCpp>);

    return c.run();
}
