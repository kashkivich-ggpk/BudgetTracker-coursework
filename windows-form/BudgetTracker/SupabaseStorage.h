#pragma once

#include "IDataStorage.h"

namespace BudgetTracker {

    using namespace System;
    using namespace System::Collections;
    using namespace System::Collections::Generic;
    using namespace System::Globalization;
    using namespace System::Net;
    using namespace System::Text;
    using namespace System::Web::Script::Serialization;

    public ref class SupabaseStorage : public IDataStorage {
    private:
        String^ projectUrl;
        String^ publishableKey;
        String^ clientId;
        JavaScriptSerializer^ json;

        Object^ GetValue(Dictionary<String^, Object^>^ item, String^ key) {
            Object^ value;
            return item != nullptr && item->TryGetValue(key, value)
                ? value
                : nullptr;
        }

        String^ TextValue(Dictionary<String^, Object^>^ item, String^ key) {
            Object^ value = GetValue(item, key);
            return value == nullptr ? String::Empty : Convert::ToString(
                value, CultureInfo::InvariantCulture);
        }

        Decimal DecimalValue(Dictionary<String^, Object^>^ item, String^ key) {
            Object^ value = GetValue(item, key);
            return value == nullptr
                ? Decimal::Zero
                : Convert::ToDecimal(value, CultureInfo::InvariantCulture);
        }

        int IntValue(Dictionary<String^, Object^>^ item, String^ key) {
            Object^ value = GetValue(item, key);
            return value == nullptr
                ? 0
                : Convert::ToInt32(value, CultureInfo::InvariantCulture);
        }

        bool BoolValue(Dictionary<String^, Object^>^ item, String^ key) {
            Object^ value = GetValue(item, key);
            return value != nullptr && Convert::ToBoolean(
                value, CultureInfo::InvariantCulture);
        }

        array<Object^>^ ArrayValue(
            Dictionary<String^, Object^>^ item,
            String^ key) {
            Object^ value = GetValue(item, key);
            return dynamic_cast<array<Object^>^>(value);
        }

        String^ CallRpc(String^ functionName, Object^ body) {
            // Все обращения к Supabase проходят через одну функцию:
            // добавляются заголовки, JSON-тело и единая обработка ошибок.
            ServicePointManager::SecurityProtocol = SecurityProtocolType::Tls12;
            WebClient^ client = gcnew WebClient();
            try {
                client->Encoding = Encoding::UTF8;
                client->Headers[HttpRequestHeader::ContentType] =
                    "application/json";
                client->Headers["apikey"] = publishableKey;
                client->Headers[HttpRequestHeader::Authorization] =
                    "Bearer " + publishableKey;
                return client->UploadString(
                    projectUrl + "/rest/v1/rpc/" + functionName,
                    "POST",
                    json->Serialize(body));
            }
            catch (WebException^ ex) {
                // Supabase часто возвращает полезное описание ошибки в теле
                // ответа, поэтому добавляем его к сообщению исключения.
                String^ details = ex->Message;
                if (ex->Response != nullptr) {
                    System::IO::StreamReader^ reader =
                        gcnew System::IO::StreamReader(
                            ex->Response->GetResponseStream(),
                            Encoding::UTF8);
                    details += "\n" + reader->ReadToEnd();
                    reader->Close();
                }
                throw gcnew InvalidOperationException(
                    L"Ошибка Supabase Data API: " + details, ex);
            }
            finally {
                delete client;
            }
        }

        array<String^>^ DefaultCategories() {
            return gcnew array<String^> {
                L"Зарплата", L"Продукты", L"Транспорт", L"Развлечения",
                L"Коммунальные платежи", L"Здоровье", L"Образование", L"Прочее"
            };
        }

    public:
        SupabaseStorage(String^ url, String^ key, String^ installationClientId) {
            if (String::IsNullOrWhiteSpace(url) ||
                String::IsNullOrWhiteSpace(key))
                throw gcnew ArgumentException(
                    L"Не заданы параметры Supabase Data API.");

            projectUrl = url->TrimEnd('/');
            publishableKey = key;
            clientId = installationClientId;
            json = gcnew JavaScriptSerializer();
            json->MaxJsonLength = Int32::MaxValue;
        }

        virtual property String^ Name {
            String^ get() { return L"Supabase"; }
        }

        virtual BudgetData^ Load() {
            // Серверная RPC-функция возвращает профиль, категории, лимиты,
            // операции и регулярные платежи одним JSON-объектом.
            Dictionary<String^, Object^>^ request =
                gcnew Dictionary<String^, Object^>();
            request["p_client_id"] = clientId;

            String^ response = CallRpc("load_budget_data", request);
            Dictionary<String^, Object^>^ root =
                json->Deserialize<Dictionary<String^, Object^>^>(response);
            BudgetData^ data = gcnew BudgetData();

            Dictionary<String^, Object^>^ profile =
                dynamic_cast<Dictionary<String^, Object^>^>(
                    GetValue(root, "profile"));
            if (profile != nullptr && profile->Count > 0) {
                data->Profile->DisplayName = TextValue(profile, "display_name");
                data->Profile->Currency = TextValue(profile, "currency");
                data->Profile->MonthlyBudget =
                    DecimalValue(profile, "monthly_budget");
                data->Profile->FinancialGoal =
                    TextValue(profile, "financial_goal");
                data->Profile->GoalAmount = DecimalValue(profile, "goal_amount");
            }

            array<Object^>^ categories = ArrayValue(root, "categories");
            if (categories != nullptr)
                for each (Object^ category in categories)
                    data->Categories->Add(category->ToString());

            array<Object^>^ limits = ArrayValue(root, "limits");
            if (limits != nullptr)
                for each (Object^ value in limits) {
                    Dictionary<String^, Object^>^ item =
                        safe_cast<Dictionary<String^, Object^>^>(value);
                    data->Limits[TextValue(item, "category")] =
                        DecimalValue(item, "amount");
                }

            array<Object^>^ transactions = ArrayValue(root, "transactions");
            if (transactions != nullptr)
                for each (Object^ value in transactions) {
                    Dictionary<String^, Object^>^ item =
                        safe_cast<Dictionary<String^, Object^>^>(value);
                    data->Transactions->Add(gcnew Transaction(
                        BoolValue(item, "is_income"),
                        DateTime::ParseExact(
                            TextValue(item, "operation_date"),
                            "yyyy-MM-dd",
                            CultureInfo::InvariantCulture),
                        DecimalValue(item, "amount"),
                        TextValue(item, "category"),
                        TextValue(item, "comment")));
                }

            array<Object^>^ recurring = ArrayValue(root, "recurring_payments");
            if (recurring != nullptr)
                for each (Object^ value in recurring) {
                    Dictionary<String^, Object^>^ item =
                        safe_cast<Dictionary<String^, Object^>^>(value);
                    RecurringPayment^ payment = gcnew RecurringPayment(
                        BoolValue(item, "is_income"),
                        DecimalValue(item, "amount"),
                        TextValue(item, "category"),
                        TextValue(item, "comment"),
                        IntValue(item, "day_of_month"),
                        DateTime::ParseExact(
                            TextValue(item, "next_payment_date"),
                            "yyyy-MM-dd",
                            CultureInfo::InvariantCulture));
                    payment->IsActive = BoolValue(item, "is_active");
                    data->RecurringPayments->Add(payment);
                }

            bool defaultsAdded = false;
            // При первом запуске добавляем стартовые категории.
            for each (String^ category in DefaultCategories())
                if (!data->Categories->Contains(category)) {
                    data->Categories->Add(category);
                    defaultsAdded = true;
                }
            if (defaultsAdded)
                Save(data);

            return data;
        }

        virtual void Save(BudgetData^ data) {
            // Собираем все данные в один JSON-запрос. Сервер сохраняет его
            // одной операцией, поэтому разделы бюджета остаются согласованными.
            Dictionary<String^, Object^>^ request =
                gcnew Dictionary<String^, Object^>();
            request["p_client_id"] = clientId;

            Dictionary<String^, Object^>^ profile =
                gcnew Dictionary<String^, Object^>();
            profile["display_name"] = data->Profile->DisplayName;
            profile["currency"] = data->Profile->Currency;
            profile["monthly_budget"] = data->Profile->MonthlyBudget;
            profile["financial_goal"] = data->Profile->FinancialGoal;
            profile["goal_amount"] = data->Profile->GoalAmount;
            request["p_profile"] = profile;
            request["p_categories"] = data->Categories->ToArray();

            List<Object^>^ limits = gcnew List<Object^>();
            for each (KeyValuePair<String^, Decimal> limit in data->Limits) {
                Dictionary<String^, Object^>^ item =
                    gcnew Dictionary<String^, Object^>();
                item["category"] = limit.Key;
                item["amount"] = limit.Value;
                limits->Add(item);
            }
            request["p_limits"] = limits->ToArray();

            List<Object^>^ transactions = gcnew List<Object^>();
            for each (Transaction^ transaction in data->Transactions) {
                Dictionary<String^, Object^>^ item =
                    gcnew Dictionary<String^, Object^>();
                item["is_income"] = transaction->IsIncome;
                item["operation_date"] = transaction->Date.ToString(
                    "yyyy-MM-dd", CultureInfo::InvariantCulture);
                item["amount"] = transaction->Amount;
                item["category"] = transaction->Category;
                item["comment"] = transaction->Comment;
                transactions->Add(item);
            }
            request["p_transactions"] = transactions->ToArray();

            List<Object^>^ recurring = gcnew List<Object^>();
            for each (RecurringPayment^ payment in data->RecurringPayments) {
                Dictionary<String^, Object^>^ item =
                    gcnew Dictionary<String^, Object^>();
                item["is_income"] = payment->IsIncome;
                item["amount"] = payment->Amount;
                item["category"] = payment->Category;
                item["comment"] = payment->Comment;
                item["day_of_month"] = payment->DayOfMonth;
                item["next_payment_date"] = payment->NextPaymentDate.ToString(
                    "yyyy-MM-dd", CultureInfo::InvariantCulture);
                item["is_active"] = payment->IsActive;
                recurring->Add(item);
            }
            request["p_recurring_payments"] = recurring->ToArray();

            CallRpc("save_budget_data", request);
        }
    };
}
