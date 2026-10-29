#pragma once

#include "IDataStorage.h"

namespace BudgetTracker {

    using namespace System;
    using namespace System::Data;
    using namespace System::Data::Odbc;
    using namespace System::Globalization;

    public ref class PostgreSqlStorage : public IDataStorage {
    private:
        String^ connectionString;
        String^ clientId;

        array<String^>^ GetDefaultCategories() {
            return gcnew array<String^> {
                L"Зарплата",
                L"Продукты",
                L"Транспорт",
                L"Развлечения",
                L"Коммунальные платежи",
                L"Здоровье",
                L"Образование",
                L"Прочее"
            };
        }

        void AddDefaultCategories(BudgetData^ data) {
            array<String^>^ defaultCategories = GetDefaultCategories();
            for each (String^ category in defaultCategories)
                if (!data->Categories->Contains(category))
                    data->Categories->Add(category);
        }

        OdbcParameter^ AddParameter(OdbcCommand^ command, OdbcType type, Object^ value) {
            OdbcParameter^ parameter = command->Parameters->Add(String::Empty, type);
            parameter->Value = value == nullptr ? DBNull::Value : value;
            return parameter;
        }

        void EnsureSchema(OdbcConnection^ connection, OdbcTransaction^ transaction) {
            // Создаёт таблицы при первом подключении. Повторный вызов безопасен
            // благодаря конструкции IF NOT EXISTS.
            String^ schemaSql =
                "CREATE TABLE IF NOT EXISTS budget_profile ("
                "client_id UUID PRIMARY KEY, display_name TEXT NOT NULL, currency TEXT NOT NULL, "
                "monthly_budget NUMERIC(18,2) NOT NULL, financial_goal TEXT NOT NULL, "
                "goal_amount NUMERIC(18,2) NOT NULL);"
                "CREATE TABLE IF NOT EXISTS budget_categories ("
                "client_id UUID NOT NULL, name TEXT NOT NULL, PRIMARY KEY(client_id, name));"
                "CREATE TABLE IF NOT EXISTS budget_limits ("
                "client_id UUID NOT NULL, category TEXT NOT NULL, amount NUMERIC(18,2) NOT NULL, "
                "PRIMARY KEY(client_id, category));"
                "CREATE TABLE IF NOT EXISTS budget_transactions ("
                "id BIGSERIAL PRIMARY KEY, client_id UUID NOT NULL, "
                "is_income BOOLEAN NOT NULL, operation_date DATE NOT NULL, "
                "amount NUMERIC(18,2) NOT NULL, category TEXT NOT NULL, comment TEXT NOT NULL DEFAULT '');"
                "CREATE TABLE IF NOT EXISTS budget_recurring_payments ("
                "id BIGSERIAL PRIMARY KEY, client_id UUID NOT NULL, "
                "is_income BOOLEAN NOT NULL, amount NUMERIC(18,2) NOT NULL, "
                "category TEXT NOT NULL, comment TEXT NOT NULL DEFAULT '', day_of_month INTEGER NOT NULL, "
                "next_payment_date DATE NOT NULL, is_active BOOLEAN NOT NULL DEFAULT TRUE);"
                "CREATE INDEX IF NOT EXISTS ix_budget_transactions_client "
                "ON budget_transactions(client_id, operation_date);"
                "CREATE INDEX IF NOT EXISTS ix_budget_recurring_client "
                "ON budget_recurring_payments(client_id);";
            OdbcCommand^ command = gcnew OdbcCommand(
                schemaSql, connection, transaction);
            command->ExecuteNonQuery();
        }

        void SaveDefaultCategories(
            OdbcConnection^ connection,
            BudgetData^ data) {
            AddDefaultCategories(data);
            String^ sql =
                "INSERT INTO budget_categories(client_id, name) VALUES "
                "(?::uuid, ?), (?::uuid, ?), (?::uuid, ?), (?::uuid, ?), "
                "(?::uuid, ?), (?::uuid, ?), (?::uuid, ?), (?::uuid, ?) "
                "ON CONFLICT DO NOTHING";
            OdbcCommand^ command = gcnew OdbcCommand(sql, connection);
            for each (String^ category in GetDefaultCategories()) {
                AddParameter(command, OdbcType::Text, clientId);
                AddParameter(command, OdbcType::Text, category);
            }
            command->ExecuteNonQuery();
            AddDefaultCategories(data);
        }

    public:
        PostgreSqlStorage(String^ value, String^ installationClientId) {
            if (String::IsNullOrWhiteSpace(value))
                throw gcnew ArgumentException(L"Не задана строка подключения PostgreSQL.");
            if (String::IsNullOrWhiteSpace(installationClientId))
                throw gcnew ArgumentException(L"Не задан идентификатор установки.");
            connectionString = value;
            clientId = installationClientId;
        }

        virtual property String^ Name {
            String^ get() { return L"PostgreSQL"; }
        }

        virtual BudgetData^ Load() {
            // UNION ALL позволяет получить все виды записей одним запросом.
            // Первый столбец содержит букву типа записи: профиль, категория,
            // лимит, операция или регулярный платёж.
            BudgetData^ data = gcnew BudgetData();
            OdbcConnection^ connection = gcnew OdbcConnection(connectionString);
            connection->Open();
            EnsureSchema(connection, nullptr);

            String^ loadSql =
                "SELECT 'P', display_name, currency, monthly_budget::text, "
                "financial_goal, goal_amount::text, NULL, NULL "
                "FROM budget_profile WHERE client_id = ?::uuid "
                "UNION ALL "
                "SELECT 'C', name, NULL, NULL, NULL, NULL, NULL, NULL "
                "FROM budget_categories WHERE client_id = ?::uuid "
                "UNION ALL "
                "SELECT 'L', category, amount::text, NULL, NULL, NULL, NULL, NULL "
                "FROM budget_limits WHERE client_id = ?::uuid "
                "UNION ALL "
                "SELECT 'T', is_income::text, operation_date::text, amount::text, "
                "category, comment, id::text, NULL "
                "FROM budget_transactions WHERE client_id = ?::uuid "
                "UNION ALL "
                "SELECT 'R', is_income::text, amount::text, category, comment, "
                "day_of_month::text, next_payment_date::text, is_active::text "
                "FROM budget_recurring_payments WHERE client_id = ?::uuid";
            OdbcCommand^ loadCommand = gcnew OdbcCommand(loadSql, connection);
            for (int parameterIndex = 0; parameterIndex < 5; parameterIndex++)
                AddParameter(loadCommand, OdbcType::Text, clientId);

            OdbcDataReader^ reader = loadCommand->ExecuteReader();
            while (reader->Read()) {
                String^ recordType = reader->GetString(0);
                if (recordType == "P") {
                    data->Profile->DisplayName = reader->GetString(1);
                    data->Profile->Currency = reader->GetString(2);
                    data->Profile->MonthlyBudget = Decimal::Parse(
                        reader->GetString(3), CultureInfo::InvariantCulture);
                    data->Profile->FinancialGoal = reader->GetString(4);
                    data->Profile->GoalAmount = Decimal::Parse(
                        reader->GetString(5), CultureInfo::InvariantCulture);
                }
                else if (recordType == "C") {
                    String^ category = reader->GetString(1);
                    if (!data->Categories->Contains(category))
                        data->Categories->Add(category);
                }
                else if (recordType == "L") {
                    data->Limits[reader->GetString(1)] = Decimal::Parse(
                        reader->GetString(2), CultureInfo::InvariantCulture);
                }
                else if (recordType == "T") {
                    String^ category = reader->GetString(4);
                    data->Transactions->Add(gcnew Transaction(
                        Boolean::Parse(reader->GetString(1)),
                        DateTime::Parse(
                            reader->GetString(2), CultureInfo::InvariantCulture),
                        Decimal::Parse(
                            reader->GetString(3), CultureInfo::InvariantCulture),
                        category,
                        reader->IsDBNull(5) ? String::Empty : reader->GetString(5)));
                    if (!data->Categories->Contains(category))
                        data->Categories->Add(category);
                }
                else if (recordType == "R") {
                    RecurringPayment^ payment = gcnew RecurringPayment(
                        Boolean::Parse(reader->GetString(1)),
                        Decimal::Parse(
                            reader->GetString(2), CultureInfo::InvariantCulture),
                        reader->GetString(3),
                        reader->IsDBNull(4) ? String::Empty : reader->GetString(4),
                        Int32::Parse(reader->GetString(5)),
                        DateTime::Parse(
                            reader->GetString(6), CultureInfo::InvariantCulture));
                    payment->IsActive = Boolean::Parse(reader->GetString(7));
                    data->RecurringPayments->Add(payment);
                    if (!data->Categories->Contains(payment->Category))
                        data->Categories->Add(payment->Category);
                }
            }
            reader->Close();

            SaveDefaultCategories(connection, data);
            connection->Close();
            return data;
        }

        virtual void Save(BudgetData^ data) {
            // Сохранение выполняется в транзакции: либо обновятся все таблицы,
            // либо при ошибке база останется в прежнем состоянии.
            OdbcConnection^ connection = gcnew OdbcConnection(connectionString);
            connection->Open();
            OdbcTransaction^ transaction = connection->BeginTransaction();
            try {
                // Для учебного проекта проще сначала удалить старый снимок
                // пользователя, а затем записать актуальное состояние целиком.
                OdbcCommand^ clearCommand = gcnew OdbcCommand(
                    "DELETE FROM budget_transactions WHERE client_id = ?::uuid;"
                    "DELETE FROM budget_recurring_payments WHERE client_id = ?::uuid;"
                    "DELETE FROM budget_limits WHERE client_id = ?::uuid;"
                    "DELETE FROM budget_categories WHERE client_id = ?::uuid;"
                    "DELETE FROM budget_profile WHERE client_id = ?::uuid",
                    connection, transaction);
                for (int parameterIndex = 0; parameterIndex < 5; parameterIndex++)
                    AddParameter(clearCommand, OdbcType::Text, clientId);
                clearCommand->ExecuteNonQuery();

                OdbcCommand^ profileCommand = gcnew OdbcCommand(
                    "INSERT INTO budget_profile"
                    "(client_id, display_name, currency, monthly_budget, financial_goal, goal_amount) "
                    "VALUES (?::uuid, ?, ?, ?, ?, ?)", connection, transaction);
                AddParameter(profileCommand, OdbcType::Text, clientId);
                AddParameter(profileCommand, OdbcType::Text, data->Profile->DisplayName);
                AddParameter(profileCommand, OdbcType::Text, data->Profile->Currency);
                AddParameter(profileCommand, OdbcType::Decimal, data->Profile->MonthlyBudget);
                AddParameter(profileCommand, OdbcType::Text, data->Profile->FinancialGoal);
                AddParameter(profileCommand, OdbcType::Decimal, data->Profile->GoalAmount);
                profileCommand->ExecuteNonQuery();

                if (data->Categories->Count > 0) {
                    System::Text::StringBuilder^ categorySql =
                        gcnew System::Text::StringBuilder(
                            "INSERT INTO budget_categories(client_id, name) VALUES ");
                    for (int index = 0; index < data->Categories->Count; index++) {
                        if (index > 0) categorySql->Append(", ");
                        categorySql->Append("(?::uuid, ?)");
                    }
                    OdbcCommand^ command = gcnew OdbcCommand(
                        categorySql->ToString(), connection, transaction);
                    for each (String^ category in data->Categories) {
                        AddParameter(command, OdbcType::Text, clientId);
                        AddParameter(command, OdbcType::Text, category);
                    }
                    command->ExecuteNonQuery();
                }

                if (data->Limits->Count > 0) {
                    System::Text::StringBuilder^ limitsSql =
                        gcnew System::Text::StringBuilder(
                            "INSERT INTO budget_limits(client_id, category, amount) VALUES ");
                    int index = 0;
                    for each (KeyValuePair<String^, Decimal> limit in data->Limits) {
                        if (index++ > 0) limitsSql->Append(", ");
                        limitsSql->Append("(?::uuid, ?, ?)");
                    }
                    OdbcCommand^ command = gcnew OdbcCommand(
                        limitsSql->ToString(), connection, transaction);
                    for each (KeyValuePair<String^, Decimal> limit in data->Limits) {
                        AddParameter(command, OdbcType::Text, clientId);
                        AddParameter(command, OdbcType::Text, limit.Key);
                        AddParameter(command, OdbcType::Decimal, limit.Value);
                    }
                    command->ExecuteNonQuery();
                }

                if (data->Transactions->Count > 0) {
                    System::Text::StringBuilder^ transactionsSql =
                        gcnew System::Text::StringBuilder(
                            "INSERT INTO budget_transactions"
                            "(client_id, is_income, operation_date, amount, category, comment) VALUES ");
                    for (int index = 0; index < data->Transactions->Count; index++) {
                        if (index > 0) transactionsSql->Append(", ");
                        transactionsSql->Append("(?::uuid, ?, ?, ?, ?, ?)");
                    }
                    OdbcCommand^ command = gcnew OdbcCommand(
                        transactionsSql->ToString(), connection, transaction);
                    for each (Transaction^ item in data->Transactions) {
                        AddParameter(command, OdbcType::Text, clientId);
                        AddParameter(command, OdbcType::Bit, item->IsIncome);
                        AddParameter(command, OdbcType::Date, item->Date.Date);
                        AddParameter(command, OdbcType::Decimal, item->Amount);
                        AddParameter(command, OdbcType::Text, item->Category);
                        AddParameter(command, OdbcType::Text, item->Comment);
                    }
                    command->ExecuteNonQuery();
                }

                if (data->RecurringPayments->Count > 0) {
                    System::Text::StringBuilder^ recurringSql =
                        gcnew System::Text::StringBuilder(
                            "INSERT INTO budget_recurring_payments"
                            "(client_id, is_income, amount, category, comment, day_of_month, "
                            "next_payment_date, is_active) VALUES ");
                    for (int index = 0; index < data->RecurringPayments->Count; index++) {
                        if (index > 0) recurringSql->Append(", ");
                        recurringSql->Append("(?::uuid, ?, ?, ?, ?, ?, ?, ?)");
                    }
                    OdbcCommand^ command = gcnew OdbcCommand(
                        recurringSql->ToString(), connection, transaction);
                    for each (RecurringPayment^ payment in data->RecurringPayments) {
                        AddParameter(command, OdbcType::Text, clientId);
                        AddParameter(command, OdbcType::Bit, payment->IsIncome);
                        AddParameter(command, OdbcType::Decimal, payment->Amount);
                        AddParameter(command, OdbcType::Text, payment->Category);
                        AddParameter(command, OdbcType::Text, payment->Comment);
                        AddParameter(command, OdbcType::Int, payment->DayOfMonth);
                        AddParameter(command, OdbcType::Date, payment->NextPaymentDate.Date);
                        AddParameter(command, OdbcType::Bit, payment->IsActive);
                    }
                    command->ExecuteNonQuery();
                }

                transaction->Commit();
            }
            catch (Exception^) {
                // Откатываем все изменения при любой ошибке записи.
                transaction->Rollback();
                throw;
            }
            finally {
                connection->Close();
            }
        }
    };
}
