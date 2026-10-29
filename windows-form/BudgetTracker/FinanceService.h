#pragma once

#include "DomainModels.h"

namespace BudgetTracker {

    using namespace System;
    using namespace System::Collections::Generic;

    // Итог по всем операциям. Баланс вычисляется, а не хранится отдельно,
    // поэтому он всегда соответствует сумме доходов и расходов.
    public ref class FinancialSummary {
    public:
        Decimal Income;
        Decimal Expense;

        property Decimal Balance {
            Decimal get() { return Income - Expense; }
        }
    };

    public ref class CategorySummary {
    public:
        Decimal Income;
        Decimal Expense;
    };

    // Финансовые расчёты вынесены из формы в отдельный простой класс.
    public ref class FinanceService abstract sealed {
    public:
        static FinancialSummary^ CalculateSummary(IEnumerable<Transaction^>^ transactions) {
            FinancialSummary^ result = gcnew FinancialSummary();
            for each (Transaction^ transaction in transactions) {
                if (transaction->IsIncome)
                    result->Income += transaction->Amount;
                else
                    result->Expense += transaction->Amount;
            }
            return result;
        }

        static Dictionary<String^, CategorySummary^>^ CalculateByCategory(
            IEnumerable<Transaction^>^ transactions,
            IEnumerable<String^>^ categories) {
            Dictionary<String^, CategorySummary^>^ result =
                gcnew Dictionary<String^, CategorySummary^>();

            // Сначала создаём пустую строку статистики для каждой категории,
            // даже если по ней пока нет операций.
            for each (String^ category in categories)
                if (!result->ContainsKey(category))
                    result[category] = gcnew CategorySummary();

            // Затем одной проходкой распределяем операции по категориям.
            for each (Transaction^ transaction in transactions) {
                if (!result->ContainsKey(transaction->Category))
                    result[transaction->Category] = gcnew CategorySummary();

                CategorySummary^ summary = result[transaction->Category];
                if (transaction->IsIncome)
                    summary->Income += transaction->Amount;
                else
                    summary->Expense += transaction->Amount;
            }

            return result;
        }

        static Decimal CalculateSpent(
            IEnumerable<Transaction^>^ transactions,
            String^ category,
            int excludedIndex) {
            Decimal spent = Decimal::Zero;
            int index = 0;
            for each (Transaction^ transaction in transactions) {
                // excludedIndex нужен при редактировании: старая сумма
                // редактируемой операции не должна учитываться дважды.
                if (index != excludedIndex && !transaction->IsIncome && transaction->Category == category)
                    spent += transaction->Amount;
                index++;
            }
            return spent;
        }

        static Dictionary<String^, Decimal>^ CalculateExpensesByCategory(
            IEnumerable<Transaction^>^ transactions) {
            Dictionary<String^, Decimal>^ result = gcnew Dictionary<String^, Decimal>();
            for each (Transaction^ transaction in transactions) {
                if (transaction->IsIncome) continue;
                if (!result->ContainsKey(transaction->Category))
                    result[transaction->Category] = Decimal::Zero;
                result[transaction->Category] += transaction->Amount;
            }
            return result;
        }
    };
}
