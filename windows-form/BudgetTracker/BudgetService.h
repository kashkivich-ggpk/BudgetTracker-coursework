#pragma once

#include "DomainModels.h"

namespace BudgetTracker {

    using namespace System;
    using namespace System::Collections::Generic;

    // Изменяет данные бюджета. UI и хранилище не должны знать детали этих операций.
    // Здесь находятся только действия над данными, без окон и сообщений.
    public ref class BudgetService abstract sealed {
    public:
        static void AddOrUpdateTransaction(
            List<Transaction^>^ transactions,
            int editingIndex,
            Transaction^ newValue) {
            if (editingIndex < 0) {
                // Индекса нет — пользователь создаёт новую операцию.
                transactions->Add(newValue);
                return;
            }

            // Индекс есть — изменяем существующий объект, сохраняя его место
            // в общем списке операций.
            Transaction^ current = transactions[editingIndex];
            current->IsIncome = newValue->IsIncome;
            current->Date = newValue->Date;
            current->Amount = newValue->Amount;
            current->Category = newValue->Category;
            current->Comment = newValue->Comment;
        }

        static void DeleteTransactions(
            List<Transaction^>^ transactions,
            List<int>^ indexes) {
            // Удаляем с конца списка. Иначе после первого удаления индексы
            // следующих элементов сдвинутся и можно удалить не те записи.
            indexes->Sort();
            indexes->Reverse();
            for each (int index in indexes)
                transactions->RemoveAt(index);
        }

        static void AddCategory(List<String^>^ categories, String^ categoryName) {
            categories->Add(categoryName);
        }

        static void RenameCategory(
            BudgetData^ data,
            String^ oldName,
            String^ newName) {
            // Название категории связано сразу с несколькими разделами.
            // Поэтому меняем справочник, операции, регулярные платежи и лимит.
            int categoryIndex = data->Categories->IndexOf(oldName);
            if (categoryIndex >= 0)
                data->Categories[categoryIndex] = newName;

            for each (Transaction^ transaction in data->Transactions)
                if (transaction->Category == oldName)
                    transaction->Category = newName;

            for each (RecurringPayment^ payment in data->RecurringPayments)
                if (payment->Category == oldName)
                    payment->Category = newName;

            if (data->Limits->ContainsKey(oldName)) {
                Decimal limitAmount = data->Limits[oldName];
                data->Limits->Remove(oldName);
                data->Limits[newName] = limitAmount;
            }
        }
    };
}
