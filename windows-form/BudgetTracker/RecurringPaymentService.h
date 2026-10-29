#pragma once

#include "DomainModels.h"

namespace BudgetTracker {

    using namespace System;
    using namespace System::Collections::Generic;

    public ref class RecurringPaymentService abstract sealed {
    private:
        static DateTime DateForMonth(int year, int month, int dayOfMonth) {
            // Для февраля и коротких месяцев день 29–31 переносится
            // на последний существующий день месяца.
            int validDay = Math::Min(dayOfMonth, DateTime::DaysInMonth(year, month));
            return DateTime(year, month, validDay);
        }

    public:
        static DateTime CalculateFirstPaymentDate(int dayOfMonth, DateTime today) {
            DateTime thisMonth = DateForMonth(today.Year, today.Month, dayOfMonth);
            if (thisMonth >= today.Date)
                return thisMonth;

            DateTime nextMonth = today.AddMonths(1);
            return DateForMonth(nextMonth.Year, nextMonth.Month, dayOfMonth);
        }

        static DateTime CalculateNextPaymentDate(
            DateTime currentDate,
            int dayOfMonth) {
            DateTime nextMonth = currentDate.AddMonths(1);
            return DateForMonth(nextMonth.Year, nextMonth.Month, dayOfMonth);
        }

        // Возвращает число автоматически созданных операций.
        // Цикл while нужен на случай, если приложение не запускали несколько
        // месяцев: будут созданы все пропущенные ежемесячные операции.
        static int GenerateDueTransactions(
            List<RecurringPayment^>^ recurringPayments,
            List<Transaction^>^ transactions,
            DateTime today) {
            int createdCount = 0;
            for each (RecurringPayment^ payment in recurringPayments) {
                if (!payment->IsActive) continue;

                while (payment->NextPaymentDate.Date <= today.Date) {
                    // Метка помогает отличить автоматически созданную запись
                    // от обычной операции при построении прогноза.
                    String^ generatedComment = String::IsNullOrWhiteSpace(payment->Comment)
                        ? L"[Регулярный платёж]"
                        : L"[Регулярный платёж] " + payment->Comment;
                    transactions->Add(gcnew Transaction(
                        payment->IsIncome,
                        payment->NextPaymentDate.Date,
                        payment->Amount,
                        payment->Category,
                        generatedComment));
                    payment->NextPaymentDate = CalculateNextPaymentDate(
                        payment->NextPaymentDate, payment->DayOfMonth);
                    createdCount++;
                }
            }
            return createdCount;
        }
    };
}
