#pragma once

namespace BudgetTracker {

    using namespace System;
    using namespace System::Collections::Generic;

    // Одна запись в истории финансов: доход или расход.
    public ref class Transaction {
    public:
        bool IsIncome;
        DateTime Date;
        Decimal Amount;
        String^ Category;
        String^ Comment;

        Transaction() {}

        Transaction(bool isIncome, DateTime date, Decimal amount, String^ category, String^ comment) {
            IsIncome = isIncome;
            Date = date;
            Amount = amount;
            Category = category;
            Comment = comment;
        }
    };

    // Шаблон операции, которая автоматически создаётся раз в месяц.
    // NextPaymentDate хранит ближайшую дату, которую ещё нужно обработать.
    public ref class RecurringPayment {
    public:
        bool IsIncome;
        Decimal Amount;
        String^ Category;
        String^ Comment;
        int DayOfMonth;
        DateTime NextPaymentDate;
        bool IsActive;

        RecurringPayment() {}

        RecurringPayment(
            bool isIncome,
            Decimal amount,
            String^ category,
            String^ comment,
            int dayOfMonth,
            DateTime nextPaymentDate) {
            IsIncome = isIncome;
            Amount = amount;
            Category = category;
            Comment = comment;
            DayOfMonth = dayOfMonth;
            NextPaymentDate = nextPaymentDate.Date;
            IsActive = true;
        }
    };

    // Настройки пользователя, которые показываются на вкладке профиля.
    public ref class PersonalProfile {
    public:
        String^ DisplayName;
        String^ Currency;
        Decimal MonthlyBudget;
        String^ FinancialGoal;
        Decimal GoalAmount;

        PersonalProfile() {
            DisplayName = L"Пользователь";
            Currency = L"BYN";
            MonthlyBudget = Decimal::Zero;
            FinancialGoal = String::Empty;
            GoalAmount = Decimal::Zero;
        }
    };

    // Все данные приложения собраны в одном объекте.
    // Такой объект удобно целиком передавать между формой и хранилищем.
    public ref class BudgetData {
    public:
        PersonalProfile^ Profile;
        List<Transaction^>^ Transactions;
        List<RecurringPayment^>^ RecurringPayments;
        List<String^>^ Categories;
        Dictionary<String^, Decimal>^ Limits;

        BudgetData() {
            Profile = gcnew PersonalProfile();
            Transactions = gcnew List<Transaction^>();
            RecurringPayments = gcnew List<RecurringPayment^>();
            Categories = gcnew List<String^>();
            Limits = gcnew Dictionary<String^, Decimal>();
        }

        bool IsEmpty() {
            return Transactions->Count == 0 &&
                RecurringPayments->Count == 0 &&
                Categories->Count == 0 &&
                Limits->Count == 0;
        }
    };
}
