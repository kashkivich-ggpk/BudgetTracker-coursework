#pragma once

#include "DomainModels.h"
#include "FinanceService.h"
#include "ForecastService.h"

namespace BudgetTracker {

    using namespace System;
    using namespace System::Collections::Generic;
    using namespace System::Globalization;
    using namespace System::IO;
    using namespace System::Text;

    // Формирует текстовый отчёт независимо от интерфейса программы.
    public ref class ReportService abstract sealed {
    private:
        static String^ Money(Decimal value) {
            return value.ToString("N2", CultureInfo::InvariantCulture);
        }

    public:
        static void ExportTextReport(
            String^ filePath,
            BudgetData^ data,
            DateTime periodFrom,
            DateTime periodTo,
            MonthForecast^ forecast) {
            // В отчёт попадают только операции выбранного периода.
            List<Transaction^>^ periodTransactions = gcnew List<Transaction^>();
            for each (Transaction^ transaction in data->Transactions)
                if (transaction->Date.Date >= periodFrom.Date &&
                    transaction->Date.Date <= periodTo.Date)
                    periodTransactions->Add(transaction);

            FinancialSummary^ summary =
                FinanceService::CalculateSummary(periodTransactions);
            Dictionary<String^, CategorySummary^>^ categorySummaries =
                FinanceService::CalculateByCategory(
                    periodTransactions, data->Categories);

            // StringBuilder удобен для последовательной сборки большого текста.
            StringBuilder^ report = gcnew StringBuilder();
            report->AppendLine(L"ОТЧЁТ ПО ЛИЧНЫМ ФИНАНСАМ");
            report->AppendLine(L"========================================");
            report->AppendLine(L"Профиль: " + data->Profile->DisplayName);
            report->AppendLine(L"Валюта: " + data->Profile->Currency);
            report->AppendLine(String::Format(
                L"Месячный бюджет: {0} {1}",
                Money(data->Profile->MonthlyBudget),
                data->Profile->Currency));
            if (!String::IsNullOrWhiteSpace(data->Profile->FinancialGoal))
                report->AppendLine(String::Format(
                    L"Финансовая цель: {0} — {1} {2}",
                    data->Profile->FinancialGoal,
                    Money(data->Profile->GoalAmount),
                    data->Profile->Currency));
            report->AppendLine(String::Format(
                L"Период: {0:dd.MM.yyyy} — {1:dd.MM.yyyy}",
                periodFrom, periodTo));
            report->AppendLine(String::Format(L"Доходы:  {0}", Money(summary->Income)));
            report->AppendLine(String::Format(L"Расходы: {0}", Money(summary->Expense)));
            report->AppendLine(String::Format(L"Итог:    {0}", Money(summary->Balance)));

            report->AppendLine();
            report->AppendLine(L"ПО КАТЕГОРИЯМ");
            report->AppendLine(L"----------------------------------------");
            for each (String^ category in data->Categories) {
                CategorySummary^ categorySummary = categorySummaries[category];
                if (categorySummary->Income == 0 && categorySummary->Expense == 0)
                    continue;
                report->AppendLine(String::Format(
                    L"{0}: доходы {1}, расходы {2}",
                    category,
                    Money(categorySummary->Income),
                    Money(categorySummary->Expense)));
            }

            report->AppendLine();
            report->AppendLine(L"ОПЕРАЦИИ");
            report->AppendLine(L"----------------------------------------");
            for each (Transaction^ transaction in periodTransactions)
                report->AppendLine(String::Format(
                    L"{0:dd.MM.yyyy} | {1} | {2} | {3} | {4}",
                    transaction->Date,
                    transaction->IsIncome ? L"Доход " : L"Расход",
                    Money(transaction->Amount),
                    transaction->Category,
                    transaction->Comment));

            report->AppendLine();
            report->AppendLine(L"ПРОГНОЗ ДО КОНЦА ТЕКУЩЕГО МЕСЯЦА");
            report->AppendLine(L"----------------------------------------");
            report->AppendLine(String::Format(
                L"Текущий баланс: {0}", Money(forecast->CurrentBalance)));
            report->AppendLine(String::Format(
                L"Ожидаемые обычные расходы: {0}",
                Money(forecast->ExpectedAdditionalExpenses)));
            report->AppendLine(String::Format(
                L"Будущие регулярные доходы: {0}",
                Money(forecast->ExpectedRecurringIncome)));
            report->AppendLine(String::Format(
                L"Будущие регулярные расходы: {0}",
                Money(forecast->ExpectedRecurringExpense)));
            report->AppendLine(String::Format(
                L"Прогнозируемый баланс: {0}", Money(forecast->ForecastBalance)));

            // UTF-8 сохраняет русские буквы независимо от настроек Windows.
            File::WriteAllText(filePath, report->ToString(), Encoding::UTF8);
        }
    };
}
