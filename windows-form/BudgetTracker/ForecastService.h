#pragma once

#include "DomainModels.h"
#include "FinanceService.h"

namespace BudgetTracker {

    using namespace System;
    using namespace System::Collections::Generic;

    // Результат прогноза вынесен в отдельный объект, чтобы форма только
    // показывала уже рассчитанные значения.
    public ref class MonthForecast {
    public:
        Decimal CurrentBalance;
        Decimal ExpectedAdditionalExpenses;
        Decimal ExpectedRecurringIncome;
        Decimal ExpectedRecurringExpense;
        Decimal ForecastBalance;
    };

    // Строит простой и понятный прогноз до конца текущего месяца.
    public ref class ForecastService abstract sealed {
    public:
        static MonthForecast^ BuildMonthForecast(
            IEnumerable<Transaction^>^ transactions,
            IEnumerable<RecurringPayment^>^ recurringPayments,
            DateTime today) {
            MonthForecast^ forecast = gcnew MonthForecast();
            forecast->CurrentBalance =
                FinanceService::CalculateSummary(transactions)->Balance;

            DateTime monthStart = DateTime(today.Year, today.Month, 1);
            int elapsedDays = Math::Max(1, today.Day);
            int remainingDays = DateTime::DaysInMonth(today.Year, today.Month) - today.Day;
            Decimal variableExpense = Decimal::Zero;

            // Берём обычные расходы текущего месяца. Автоматические платежи
            // исключаем, потому что ниже они учитываются отдельно.
            for each (Transaction^ transaction in transactions) {
                bool isCurrentMonth =
                    transaction->Date.Date >= monthStart &&
                    transaction->Date.Date <= today.Date;
                bool isGeneratedRecurring =
                    transaction->Comment != nullptr &&
                    transaction->Comment->StartsWith(L"[Регулярный платёж]");
                if (isCurrentMonth && !transaction->IsIncome && !isGeneratedRecurring)
                    variableExpense += transaction->Amount;
            }

            Decimal averageDailyExpense = variableExpense / elapsedDays;
            // Предполагаем, что средний расход в оставшиеся дни сохранится.
            forecast->ExpectedAdditionalExpenses =
                Decimal::Round(averageDailyExpense * remainingDays, 2);

            DateTime monthEnd = DateTime(
                today.Year,
                today.Month,
                DateTime::DaysInMonth(today.Year, today.Month));
            // Добавляем только те регулярные платежи, которые ещё произойдут
            // до конца текущего месяца.
            for each (RecurringPayment^ payment in recurringPayments) {
                if (!payment->IsActive ||
                    payment->NextPaymentDate.Date <= today.Date ||
                    payment->NextPaymentDate.Date > monthEnd)
                    continue;

                if (payment->IsIncome)
                    forecast->ExpectedRecurringIncome += payment->Amount;
                else
                    forecast->ExpectedRecurringExpense += payment->Amount;
            }

            // Итоговая формула прогноза.
            forecast->ForecastBalance =
                forecast->CurrentBalance +
                forecast->ExpectedRecurringIncome -
                forecast->ExpectedRecurringExpense -
                forecast->ExpectedAdditionalExpenses;
            return forecast;
        }
    };
}
