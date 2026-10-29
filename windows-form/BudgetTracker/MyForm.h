    #pragma once

    #include "DomainModels.h"
    #include "BudgetService.h"
    #include "FinanceService.h"
    #include "ForecastService.h"
    #include "InputValidator.h"
    #include "RecurringPaymentService.h"
    #include "ReportService.h"
    #include "StorageFactory.h"

    namespace BudgetTracker {

        using namespace System;
        using namespace System::ComponentModel;
        using namespace System::Collections::Generic;
        using namespace System::Windows::Forms;
        using namespace System::Drawing;
        using namespace System::Globalization;

        // ------------------------------------------------------------------
        // Главная форма приложения "Учёт личных финансов".
        // ------------------------------------------------------------------
        public ref class MyForm : public System::Windows::Forms::Form {
        public:
            MyForm(void) {
                // Сначала создаём окно, затем загружаем данные
                // и только после этого заполняем элементы интерфейса.
                InitializeComponent();
                ApplyModernStyle();
                LoadData();
                ShowLoadedData();
            }

        protected:
            ~MyForm() {
                if (components) { delete components; }
            }

        private:
            void ShowLoadedData() {
                LoadProfileIntoForm();
                RefreshCategoryCombos();
                RefreshCategoryList();
                RefreshRecurringPayments();
                RefreshAll();
            }

            // ---- данные ----
            List<Transaction^>^        transactions;
            List<RecurringPayment^>^   recurringPayments;
            PersonalProfile^           profile;
            List<String^>^             categories;
            Dictionary<String^, Decimal>^ limits;
            IDataStorage^ storage;
            bool storageReady;
            bool      filterActive;
            DateTime  filterFrom;
            DateTime  filterTo;
            int       editingIndex;   // -1 = режим добавления; иначе индекс редактируемой операции

            System::ComponentModel::IContainer^ components;

            // ---- элементы управления ----
            TabControl^ tabControl;
            TabPage^ tabOperations;
            TabPage^ tabProfile;
            TabPage^ tabCategories;
            TabPage^ tabRecurring;
            TabPage^ tabStats;

            // вкладка "Операции"
            GroupBox^ grpNew;
            Label^ lblType;          ComboBox^ cmbType;
            Label^ lblAmount;        TextBox^ txtAmount;
            Label^ lblCategory;      ComboBox^ cmbCategory;
            Label^ lblDate;          DateTimePicker^ dtpDate;
            Label^ lblComment;       TextBox^ txtComment;
            Button^ btnAdd;
            GroupBox^ grpFilter;
            Label^ lblFrom;          DateTimePicker^ dtpFrom;
            Label^ lblTo;            DateTimePicker^ dtpTo;
            Button^ btnApplyFilter;  Button^ btnResetFilter;
            Label^ lblBalance;
            Label^ lblStorage;
            Button^ btnEdit;         Button^ btnDelete;
            DataGridView^ grid;
            Label^ lblGreeting;

            // вкладка "Категории и лимиты"
            GroupBox^ grpCat;
            ListBox^ lstCategories;
            Label^ lblNewCat;        TextBox^ txtNewCategory;
            Button^ btnAddCategory;  Button^ btnRenameCategory;
            GroupBox^ grpLimit;
            Label^ lblLimCat;        ComboBox^ cmbLimitCategory;
            Label^ lblLimAmount;     TextBox^ txtLimitAmount;
            Button^ btnSetLimit;     Button^ btnRemoveLimit;
            DataGridView^ gridLimits;

            // вкладка "Личный профиль"
            TextBox^ txtProfileName;
            ComboBox^ cmbProfileCurrency;
            TextBox^ txtMonthlyBudget;
            TextBox^ txtFinancialGoal;
            TextBox^ txtGoalAmount;
            Label^ lblGoalProgress;
            Label^ lblStorageDetails;
            Button^ btnSaveProfile;

            // вкладка "Регулярные платежи"
            ComboBox^ cmbRecurringType;
            TextBox^ txtRecurringAmount;
            ComboBox^ cmbRecurringCategory;
            NumericUpDown^ numRecurringDay;
            TextBox^ txtRecurringComment;
            Button^ btnAddRecurring;
            Button^ btnDeleteRecurring;
            DataGridView^ gridRecurring;

            // вкладка "Статистика"
            DataGridView^ gridAgg;
            Label^ lblChartTitle;
            Label^ lblForecast;
            Button^ btnExportReport;
            Panel^ chartPanel;

            // ==============================================================
            // 4. Работа с данными: загрузка и сохранение
            // ==============================================================
            BudgetData^ CreateDataSnapshot() {
                // Собираем текущее состояние в один объект для сохранения.
                BudgetData^ data = gcnew BudgetData();
                data->Profile = profile;
                data->Transactions = transactions;
                data->RecurringPayments = recurringPayments;
                data->Categories = categories;
                data->Limits = limits;
                return data;
            }

            void UpdateStorageStatus() {
                String^ storageName = storage == nullptr ? L"не определено" : storage->Name;
                this->Text = L"Финансовый помощник";
                if (lblStorage != nullptr)
                    lblStorage->Text = L"Хранилище: " + storageName;
                if (lblStorageDetails != nullptr) {
                    lblStorageDetails->Text =
                        L"Все данные сохраняются только в локальной базе PostgreSQL.\n"
                        L"При недоступности базы приложение не переходит на текстовый файл.";
                }
            }

            void LoadProfileIntoForm() {
                txtProfileName->Text = profile->DisplayName;
                if (!cmbProfileCurrency->Items->Contains(profile->Currency))
                    cmbProfileCurrency->Items->Add(profile->Currency);
                cmbProfileCurrency->SelectedItem = profile->Currency;
                txtMonthlyBudget->Text =
                    profile->MonthlyBudget.ToString(CultureInfo::InvariantCulture);
                txtFinancialGoal->Text = profile->FinancialGoal;
                txtGoalAmount->Text =
                    profile->GoalAmount.ToString(CultureInfo::InvariantCulture);
                RefreshProfileSummary();
            }

            void RefreshProfileSummary() {
                String^ displayName = String::IsNullOrWhiteSpace(profile->DisplayName)
                    ? L"Пользователь"
                    : profile->DisplayName;
                lblGreeting->Text = L"Здравствуйте, " + displayName + L"!";

                Decimal balance = transactions == nullptr
                    ? Decimal::Zero
                    : FinanceService::CalculateSummary(transactions)->Balance;
                if (profile->GoalAmount <= Decimal::Zero ||
                    String::IsNullOrWhiteSpace(profile->FinancialGoal)) {
                    lblGoalProgress->Text =
                        L"Укажите финансовую цель и сумму — здесь появится прогресс.";
                    return;
                }

                Decimal progress = balance > Decimal::Zero
                    ? balance
                    : Decimal::Zero;
                Decimal percent =
                    Decimal::Round(progress / profile->GoalAmount * 100, 1);
                if (percent > 100) percent = 100;
                lblGoalProgress->Text = String::Format(
                    L"Цель «{0}»: {1} из {2} {3} ({4}%)",
                    profile->FinancialGoal,
                    progress.ToString("N2", CultureInfo::InvariantCulture),
                    profile->GoalAmount.ToString("N2", CultureInfo::InvariantCulture),
                    profile->Currency,
                    percent);
            }

            void LoadData() {
                editingIndex = -1;
                filterActive = false;
                storageReady = false;
                try {
                    // Фабрика читает storage.config и создаёт хранилище.
                    storage = StorageFactory::Create();
                    BudgetData^ data = storage->Load();
                    transactions = data->Transactions;
                    recurringPayments = data->RecurringPayments;
                    profile = data->Profile;
                    categories = data->Categories;
                    limits = data->Limits;
                    int generatedCount = RecurringPaymentService::GenerateDueTransactions(
                        recurringPayments, transactions, DateTime::Today);
                    // Сразу сохраняем автоматически созданные платежи.
                    if (generatedCount > 0)
                        storage->Save(CreateDataSnapshot());
                    storageReady = true;
                    UpdateStorageStatus();
                }
                catch (Exception^ ex) {
                    // Пустые коллекции защищают форму от ошибок nullptr,
                    // а вкладки отключаются, чтобы данные нельзя было менять.
                    transactions = gcnew List<Transaction^>();
                    recurringPayments = gcnew List<RecurringPayment^>();
                    profile = gcnew PersonalProfile();
                    categories = gcnew List<String^>();
                    categories->Add(L"Прочее");
                    limits = gcnew Dictionary<String^, Decimal>();
                    tabControl->Enabled = false;
                    MessageBox::Show(
                        L"Не удалось подключиться к PostgreSQL.\n\n" + ex->Message +
                        L"\n\nРабота с данными отключена до восстановления подключения.",
                        L"PostgreSQL недоступен",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Error);
                }
            }

            void SaveData() {
                // Если загрузка не удалась, пустые данные сохранять нельзя.
                if (!storageReady) return;
                try {
                    storage->Save(CreateDataSnapshot());
                    UpdateStorageStatus();
                }
                catch (Exception^ ex) {
                    MessageBox::Show(L"Ошибка сохранения: " + ex->Message,
                                     L"Ошибка", MessageBoxButtons::OK, MessageBoxIcon::Error);
                }
            }

            // ==============================================================
            // Обновление визуальных списков
            // ==============================================================

            // 1. Отображение данных + 3. Визуальное разграничение (цвет)
            void RefreshGrid() {
                grid->Rows->Clear();
                for (int transactionIndex = 0; transactionIndex < transactions->Count; transactionIndex++) {
                    Transaction^ transaction = transactions[transactionIndex];
                    // Фильтр скрывает строки, но не удаляет сами операции.
                    if (filterActive) {
                        if (transaction->Date.Date < filterFrom.Date ||
                            transaction->Date.Date > filterTo.Date)
                            continue;
                    }
                    int rowIndex = grid->Rows->Add();
                    DataGridViewRow^ row = grid->Rows[rowIndex];
                    // Tag связывает строку таблицы с индексом исходной операции.
                    row->Tag = transactionIndex;
                    row->Cells[0]->Value = transaction->IsIncome ? L"Доход" : L"Расход";
                    row->Cells[1]->Value = transaction->Date.ToString("dd.MM.yyyy");
                    row->Cells[2]->Value =
                        transaction->Amount.ToString("N2", CultureInfo::InvariantCulture);
                    row->Cells[3]->Value = transaction->Category;
                    row->Cells[4]->Value = transaction->Comment;
                    if (transaction->IsIncome) {
                        row->DefaultCellStyle->BackColor = Color::FromArgb(223, 245, 223);
                        row->DefaultCellStyle->ForeColor = Color::FromArgb(0, 110, 0);
                    } else {
                        row->DefaultCellStyle->BackColor = Color::FromArgb(250, 222, 222);
                        row->DefaultCellStyle->ForeColor = Color::FromArgb(160, 0, 0);
                    }
                }
            }

            // 2. Расчёт финансового результата (баланс по всем операциям)
            void RefreshBalance() {
                FinancialSummary^ summary = FinanceService::CalculateSummary(transactions);
                Decimal balance = summary->Balance;
            lblBalance->Text = String::Format(
                L"Баланс: {0}      Доходы: {1}      Расходы: {2}",
                    balance.ToString("N2", CultureInfo::InvariantCulture),
                    summary->Income.ToString("N2", CultureInfo::InvariantCulture),
                    summary->Expense.ToString("N2", CultureInfo::InvariantCulture));
                lblBalance->ForeColor =
                    balance >= 0 ? Color::FromArgb(0, 110, 0) : Color::FromArgb(170, 0, 0);
            }

            void RefreshCategoryCombos() {
                String^ previousCategory = cmbCategory->SelectedItem == nullptr
                    ? nullptr
                    : cmbCategory->SelectedItem->ToString();
                cmbCategory->Items->Clear();
                cmbLimitCategory->Items->Clear();
                cmbRecurringCategory->Items->Clear();
                for each (String^ category in categories) {
                    cmbCategory->Items->Add(category);
                    cmbLimitCategory->Items->Add(category);
                    cmbRecurringCategory->Items->Add(category);
                }
            if (previousCategory != nullptr && cmbCategory->Items->Contains(previousCategory))
                cmbCategory->SelectedItem = previousCategory;
            else if (cmbCategory->Items->Count > 0)
                cmbCategory->SelectedIndex = 0;
            if (cmbLimitCategory->Items->Count > 0 && cmbLimitCategory->SelectedIndex < 0)
                cmbLimitCategory->SelectedIndex = 0;
            if (cmbRecurringCategory->Items->Count > 0 &&
                cmbRecurringCategory->SelectedIndex < 0)
                cmbRecurringCategory->SelectedIndex = 0;
            }

            void RefreshCategoryList() {
                lstCategories->Items->Clear();
            for each (String^ category in categories)
                lstCategories->Items->Add(category);
            }

            // 2. Агрегация: суммарные значения по каждой категории
        void RefreshAggregation() {
            gridAgg->Rows->Clear();
            Dictionary<String^, CategorySummary^>^ summaries =
                FinanceService::CalculateByCategory(transactions, categories);
            for each (String^ category in categories) {
                CategorySummary^ summary = summaries[category];
                int rowIndex = gridAgg->Rows->Add();
                gridAgg->Rows[rowIndex]->Cells[0]->Value = category;
                    gridAgg->Rows[rowIndex]->Cells[1]->Value =
                        summary->Income.ToString("N2", CultureInfo::InvariantCulture);
                gridAgg->Rows[rowIndex]->Cells[2]->Value =
                    summary->Expense.ToString("N2", CultureInfo::InvariantCulture);
            }
            }

            // 3. Управление лимитами: список лимитов с остатком
            void RefreshLimitsList() {
                gridLimits->Rows->Clear();
                for each (KeyValuePair<String^, Decimal> categoryLimit in limits) {
                    Decimal spent = FinanceService::CalculateSpent(
                        transactions, categoryLimit.Key, -1);
                    Decimal remaining = categoryLimit.Value - spent;
                    int rowIndex = gridLimits->Rows->Add();
                    gridLimits->Rows[rowIndex]->Cells[0]->Value = categoryLimit.Key;
                    gridLimits->Rows[rowIndex]->Cells[1]->Value =
                        categoryLimit.Value.ToString("N2", CultureInfo::InvariantCulture);
                    gridLimits->Rows[rowIndex]->Cells[2]->Value =
                        spent.ToString("N2", CultureInfo::InvariantCulture);
                    gridLimits->Rows[rowIndex]->Cells[3]->Value =
                        remaining.ToString("N2", CultureInfo::InvariantCulture);
                    // Красный цвет — лимит превышен, жёлтый — использовано 80%.
                    if (spent > categoryLimit.Value)
                        gridLimits->Rows[rowIndex]->DefaultCellStyle->BackColor =
                            Color::FromArgb(250, 210, 210);
                    else if (spent * 5 >= categoryLimit.Value * 4)
                        gridLimits->Rows[rowIndex]->DefaultCellStyle->BackColor =
                            Color::FromArgb(255, 245, 200);
                }
            }

            void RefreshRecurringPayments() {
                gridRecurring->Rows->Clear();
                for (int paymentIndex = 0;
                     paymentIndex < recurringPayments->Count;
                     paymentIndex++) {
                    RecurringPayment^ payment = recurringPayments[paymentIndex];
                    int rowIndex = gridRecurring->Rows->Add();
                    DataGridViewRow^ row = gridRecurring->Rows[rowIndex];
                    row->Tag = paymentIndex;
                    row->Cells[0]->Value = payment->IsIncome ? L"Доход" : L"Расход";
                    row->Cells[1]->Value =
                        payment->Amount.ToString("N2", CultureInfo::InvariantCulture);
                    row->Cells[2]->Value = payment->Category;
                    row->Cells[3]->Value = payment->DayOfMonth;
                    row->Cells[4]->Value =
                        payment->NextPaymentDate.ToString("dd.MM.yyyy");
                    row->Cells[5]->Value = payment->Comment;
                }
            }

            MonthForecast^ BuildForecast() {
                return ForecastService::BuildMonthForecast(
                    transactions, recurringPayments, DateTime::Today);
            }

            void RefreshForecast() {
                MonthForecast^ forecast = BuildForecast();
                lblForecast->Text = String::Format(
                    L"Прогноз баланса на конец месяца: {0}\n"
                    L"Ожидаемые расходы: {1}; регулярные доходы: {2}; регулярные расходы: {3}",
                    forecast->ForecastBalance.ToString(
                        "N2", CultureInfo::InvariantCulture),
                    forecast->ExpectedAdditionalExpenses.ToString(
                        "N2", CultureInfo::InvariantCulture),
                    forecast->ExpectedRecurringIncome.ToString(
                        "N2", CultureInfo::InvariantCulture),
                    forecast->ExpectedRecurringExpense.ToString(
                        "N2", CultureInfo::InvariantCulture));
                lblForecast->ForeColor = forecast->ForecastBalance >= 0
                    ? Color::FromArgb(0, 110, 0)
                    : Color::FromArgb(170, 0, 0);
            }

            // Возвращает даты в правильном порядке.
            // Используется и фильтром, и экспортом отчёта.
            void PutDatesInOrder(DateTime% dateFrom, DateTime% dateTo) {
                if (dateFrom <= dateTo) return;

                DateTime temporaryDate = dateFrom;
                dateFrom = dateTo;
                dateTo = temporaryDate;
            }

            // Проверяет поля операции и создаёт готовый объект.
            bool TryReadTransactionFromForm(Transaction^% transaction) {
                Decimal amount;
                if (!InputValidator::TryParsePositiveAmount(txtAmount->Text, amount)) {
                    MessageBox::Show(
                        L"Введите корректную положительную сумму.",
                        L"Ошибка ввода",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Warning);
                    return false;
                }

                if (cmbCategory->SelectedItem == nullptr) {
                    MessageBox::Show(
                        L"Выберите категорию.",
                        L"Ошибка ввода",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Warning);
                    return false;
                }

                transaction = gcnew Transaction(
                    cmbType->SelectedIndex == 0,
                    dtpDate->Value.Date,
                    amount,
                    cmbCategory->SelectedItem->ToString(),
                    InputValidator::NormalizeComment(txtComment->Text));
                return true;
            }

            // Если расход превышает лимит, спрашивает подтверждение пользователя.
            bool CanSaveTransaction(Transaction^ transaction) {
                if (transaction->IsIncome ||
                    !limits->ContainsKey(transaction->Category))
                    return true;

                Decimal spent = FinanceService::CalculateSpent(
                    transactions, transaction->Category, editingIndex);
                Decimal categoryLimit = limits[transaction->Category];
                if (spent + transaction->Amount <= categoryLimit)
                    return true;

                System::Windows::Forms::DialogResult answer = MessageBox::Show(
                    String::Format(
                        L"Внимание! Превышен лимит по категории «{0}».\n\n"
                        L"Лимит: {1}\nУже потрачено: {2}\n"
                        L"С учётом новой операции: {3}\n\n"
                        L"Всё равно добавить операцию?",
                        transaction->Category,
                        categoryLimit.ToString("N2", CultureInfo::InvariantCulture),
                        spent.ToString("N2", CultureInfo::InvariantCulture),
                        (spent + transaction->Amount).ToString(
                            "N2", CultureInfo::InvariantCulture)),
                    L"Превышение лимита",
                    MessageBoxButtons::YesNo,
                    MessageBoxIcon::Warning);
                return answer == System::Windows::Forms::DialogResult::Yes;
            }

            void ResetTransactionForm() {
                editingIndex = -1;
                btnAdd->Text = L"Добавить";
                txtAmount->Clear();
                txtComment->Clear();
            }

            // Общий финал любого изменения: обновить экран и сохранить данные.
            void FinishDataChange() {
                RefreshAll();
                SaveData();
            }

            // ==============================================================
            // Обработчики событий
            // ==============================================================

            // 1. Регистрация транзакции (и сохранение при редактировании)
            System::Void btnAdd_Click(System::Object^ sender, System::EventArgs^ e) {
                Transaction^ transaction;
                if (!TryReadTransactionFromForm(transaction)) return;
                if (!CanSaveTransaction(transaction)) return;

                BudgetService::AddOrUpdateTransaction(
                    transactions, editingIndex, transaction);
                ResetTransactionForm();
                FinishDataChange();
            }

            // 2. Временная фильтрация
        System::Void btnApplyFilter_Click(System::Object^ sender, System::EventArgs^ e) {
                filterFrom = dtpFrom->Value.Date;
                filterTo   = dtpTo->Value.Date;
                PutDatesInOrder(filterFrom, filterTo);
                filterActive = true;
                RefreshGrid();
            }

            System::Void btnResetFilter_Click(System::Object^ sender, System::EventArgs^ e) {
                filterActive = false;
                RefreshGrid();
            }

            System::Void btnEdit_Click(System::Object^ sender, System::EventArgs^ e) {
                if (grid->CurrentRow == nullptr || grid->CurrentRow->Tag == nullptr) {
                    MessageBox::Show(L"Выберите операцию в таблице.",
                                     L"Редактирование", MessageBoxButtons::OK, MessageBoxIcon::Information);
                    return;
                }
                int transactionIndex = safe_cast<int>(grid->CurrentRow->Tag);
                Transaction^ transaction = transactions[transactionIndex];
                cmbType->SelectedIndex = transaction->IsIncome ? 0 : 1;
                txtAmount->Text = transaction->Amount.ToString(CultureInfo::InvariantCulture);
                if (!cmbCategory->Items->Contains(transaction->Category))
                    cmbCategory->Items->Add(transaction->Category);
                cmbCategory->SelectedItem = transaction->Category;
                dtpDate->Value = transaction->Date;
                txtComment->Text = transaction->Comment;
                editingIndex = transactionIndex;
                btnAdd->Text = L"Сохранить изменения";
                tabControl->SelectedTab = tabOperations;
            }

            System::Void btnDelete_Click(System::Object^ sender, System::EventArgs^ e) {
                if (grid->SelectedRows->Count == 0) {
                    MessageBox::Show(L"Выберите операцию (или несколько) для удаления.",
                                     L"Удаление", MessageBoxButtons::OK, MessageBoxIcon::Information);
                    return;
                }
                List<int>^ selectedIndexes = gcnew List<int>();
                for each (DataGridViewRow^ selectedRow in grid->SelectedRows)
                    if (selectedRow->Tag != nullptr)
                        selectedIndexes->Add(safe_cast<int>(selectedRow->Tag));
                BudgetService::DeleteTransactions(transactions, selectedIndexes);
                ResetTransactionForm();
                FinishDataChange();
            }

            // 4. Управление справочниками: добавление категории
            System::Void btnAddCategory_Click(System::Object^ sender, System::EventArgs^ e) {
                String^ categoryName =
                    InputValidator::NormalizeCategoryName(txtNewCategory->Text);
                if (String::IsNullOrEmpty(categoryName)) {
                    MessageBox::Show(L"Введите название категории.",
                                     L"Категории", MessageBoxButtons::OK, MessageBoxIcon::Warning);
                    return;
                }
                if (categories->Contains(categoryName)) {
                    MessageBox::Show(L"Такая категория уже существует.",
                                     L"Категории", MessageBoxButtons::OK, MessageBoxIcon::Information);
                    return;
                }
                BudgetService::AddCategory(categories, categoryName);
                txtNewCategory->Clear();
                RefreshCategoryCombos();
                RefreshCategoryList();
                RefreshAggregation();
                SaveData();
            }

            // 4. Управление справочниками: переименование категории
            System::Void btnRenameCategory_Click(System::Object^ sender, System::EventArgs^ e) {
                if (lstCategories->SelectedItem == nullptr) {
                    MessageBox::Show(L"Выберите категорию в списке для переименования.",
                                     L"Категории", MessageBoxButtons::OK, MessageBoxIcon::Information);
                    return;
                }
                String^ newName =
                    InputValidator::NormalizeCategoryName(txtNewCategory->Text);
                if (String::IsNullOrEmpty(newName)) {
                    MessageBox::Show(L"Введите новое название в поле «Название категории».",
                                     L"Категории", MessageBoxButtons::OK, MessageBoxIcon::Warning);
                    return;
                }
                String^ oldName = lstCategories->SelectedItem->ToString();
                if (oldName == newName) return;
                if (categories->Contains(newName)) {
                    MessageBox::Show(L"Категория с таким названием уже существует.",
                                     L"Категории", MessageBoxButtons::OK, MessageBoxIcon::Information);
                    return;
                }
                BudgetService::RenameCategory(CreateDataSnapshot(), oldName, newName);
                txtNewCategory->Clear();
                RefreshCategoryCombos();
                RefreshCategoryList();
                FinishDataChange();
            }

            // 3. Управление лимитами: установка лимита
            System::Void btnSetLimit_Click(System::Object^ sender, System::EventArgs^ e) {
                if (cmbLimitCategory->SelectedItem == nullptr) {
                    MessageBox::Show(L"Выберите категорию.",
                                     L"Лимиты", MessageBoxButtons::OK, MessageBoxIcon::Warning);
                    return;
                }
                Decimal limitAmount;
                if (!InputValidator::TryParseNonNegativeAmount(
                        txtLimitAmount->Text, limitAmount)) {
                    MessageBox::Show(L"Введите корректное значение лимита (число >= 0).",
                                     L"Лимиты", MessageBoxButtons::OK, MessageBoxIcon::Warning);
                    return;
                }
                String^ category = cmbLimitCategory->SelectedItem->ToString();
                limits[category] = limitAmount;
                txtLimitAmount->Clear();
                RefreshLimitsList();
                SaveData();
            }

            System::Void btnRemoveLimit_Click(System::Object^ sender, System::EventArgs^ e) {
                if (gridLimits->CurrentRow == nullptr || gridLimits->CurrentRow->Cells[0]->Value == nullptr) {
                    MessageBox::Show(L"Выберите лимит для удаления.",
                                     L"Лимиты", MessageBoxButtons::OK, MessageBoxIcon::Information);
                    return;
                }
                String^ category = gridLimits->CurrentRow->Cells[0]->Value->ToString();
                if (limits->ContainsKey(category))
                    limits->Remove(category);
                RefreshLimitsList();
                SaveData();
            }

            System::Void btnSaveProfile_Click(
                System::Object^ sender,
                System::EventArgs^ e) {
                Decimal monthlyBudget;
                Decimal goalAmount;
                if (!InputValidator::TryParseNonNegativeAmount(
                        txtMonthlyBudget->Text, monthlyBudget) ||
                    !InputValidator::TryParseNonNegativeAmount(
                        txtGoalAmount->Text, goalAmount)) {
                    MessageBox::Show(
                        L"Бюджет и сумма цели должны быть числами не меньше нуля.",
                        L"Личный профиль",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Warning);
                    return;
                }

                String^ displayName = txtProfileName->Text->Trim();
                profile->DisplayName = String::IsNullOrWhiteSpace(displayName)
                    ? L"Пользователь"
                    : displayName;
                profile->Currency = cmbProfileCurrency->SelectedItem == nullptr
                    ? L"BYN"
                    : cmbProfileCurrency->SelectedItem->ToString();
                profile->MonthlyBudget = monthlyBudget;
                profile->FinancialGoal = txtFinancialGoal->Text->Trim();
                profile->GoalAmount = goalAmount;

                RefreshProfileSummary();
                RefreshForecast();
                SaveData();
                MessageBox::Show(
                    L"Профиль сохранён.",
                    L"Личный профиль",
                    MessageBoxButtons::OK,
                    MessageBoxIcon::Information);
            }

            System::Void btnAddRecurring_Click(
                System::Object^ sender,
                System::EventArgs^ e) {
                Decimal amount;
                if (!InputValidator::TryParsePositiveAmount(
                        txtRecurringAmount->Text, amount)) {
                    MessageBox::Show(L"Введите корректную положительную сумму.",
                        L"Регулярные платежи",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Warning);
                    return;
                }
                if (cmbRecurringCategory->SelectedItem == nullptr) {
                    MessageBox::Show(L"Выберите категорию.",
                        L"Регулярные платежи",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Warning);
                    return;
                }

                int dayOfMonth = Decimal::ToInt32(numRecurringDay->Value);
                DateTime firstPaymentDate =
                    RecurringPaymentService::CalculateFirstPaymentDate(
                        dayOfMonth, DateTime::Today);
                RecurringPayment^ payment = gcnew RecurringPayment(
                    cmbRecurringType->SelectedIndex == 0,
                    amount,
                    cmbRecurringCategory->SelectedItem->ToString(),
                    InputValidator::NormalizeComment(txtRecurringComment->Text),
                    dayOfMonth,
                    firstPaymentDate);
                recurringPayments->Add(payment);

                RecurringPaymentService::GenerateDueTransactions(
                    recurringPayments, transactions, DateTime::Today);
                txtRecurringAmount->Clear();
                txtRecurringComment->Clear();
                RefreshRecurringPayments();
                RefreshAll();
                SaveData();
            }

            System::Void btnDeleteRecurring_Click(
                System::Object^ sender,
                System::EventArgs^ e) {
                if (gridRecurring->CurrentRow == nullptr ||
                    gridRecurring->CurrentRow->Tag == nullptr) {
                    MessageBox::Show(L"Выберите регулярный платёж для удаления.",
                        L"Регулярные платежи",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Information);
                    return;
                }

                int paymentIndex =
                    safe_cast<int>(gridRecurring->CurrentRow->Tag);
                recurringPayments->RemoveAt(paymentIndex);
                RefreshRecurringPayments();
                RefreshForecast();
                SaveData();
            }

            System::Void btnExportReport_Click(
                System::Object^ sender,
                System::EventArgs^ e) {
                DateTime periodFrom = dtpFrom->Value.Date;
                DateTime periodTo = dtpTo->Value.Date;
                PutDatesInOrder(periodFrom, periodTo);

                SaveFileDialog^ dialog = gcnew SaveFileDialog();
                dialog->Title = L"Сохранить отчёт";
                dialog->Filter = L"Текстовый отчёт (*.txt)|*.txt";
                dialog->FileName = String::Format(
                    L"budget_report_{0:yyyyMMdd}_{1:yyyyMMdd}.txt",
                    periodFrom, periodTo);
                if (dialog->ShowDialog() !=
                    System::Windows::Forms::DialogResult::OK)
                    return;

                try {
                    ReportService::ExportTextReport(
                        dialog->FileName,
                        CreateDataSnapshot(),
                        periodFrom,
                        periodTo,
                        BuildForecast());
                    MessageBox::Show(L"Отчёт успешно сохранён.",
                        L"Экспорт отчёта",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Information);
                }
                catch (Exception^ ex) {
                    MessageBox::Show(L"Не удалось сохранить отчёт: " + ex->Message,
                        L"Экспорт отчёта",
                        MessageBoxButtons::OK,
                        MessageBoxIcon::Error);
                }
            }

            System::Void tabControl_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e) {
                if (transactions == nullptr) return;
                if (tabControl->SelectedTab == tabStats) {
                    RefreshAggregation();
                    RefreshForecast();
                    chartPanel->Invalidate();
                }
            }

            // 5. Графическая отчётность: круговая диаграмма расходов по категориям
            System::Void chartPanel_Paint(System::Object^ sender, System::Windows::Forms::PaintEventArgs^ e) {
                if (transactions == nullptr) return;
                // Диаграмма рисуется средствами GDI+ при перерисовке панели.
                Graphics^ graphics = e->Graphics;
                graphics->SmoothingMode = System::Drawing::Drawing2D::SmoothingMode::AntiAlias;
                graphics->Clear(chartPanel->BackColor);

            Dictionary<String^, Decimal>^ expensesByCategory =
                FinanceService::CalculateExpensesByCategory(transactions);
                Decimal totalExpense = Decimal::Zero;
                for each (KeyValuePair<String^, Decimal> categoryExpense in expensesByCategory)
                    totalExpense += categoryExpense.Value;

                if (totalExpense <= 0) {
                    graphics->DrawString(L"Нет данных о расходах для построения диаграммы.",
                        gcnew System::Drawing::Font("Segoe UI", 10),
                        Brushes::Gray, 20.0f, 20.0f);
                    return;
                }

                array<Color>^ palette = {
                    Color::FromArgb(78,121,167),  Color::FromArgb(242,142,43),
                    Color::FromArgb(225,87,89),   Color::FromArgb(118,183,178),
                    Color::FromArgb(89,161,79),   Color::FromArgb(237,201,72),
                    Color::FromArgb(176,122,161), Color::FromArgb(255,157,167),
                    Color::FromArgb(156,117,95),  Color::FromArgb(186,176,172)
                };

            int chartSize = Math::Min(chartPanel->Height - 40, chartPanel->Width - 230);
            if (chartSize < 60) chartSize = 60;
            Rectangle chartBounds = Rectangle(20, 20, chartSize, chartSize);

                double totalExpenseValue = Decimal::ToDouble(totalExpense);
                float startAngle = 0.0f;
                int colorIndex = 0;
                int legendX = chartBounds.Right + 25;
                int legendY = 25;

                for each (KeyValuePair<String^, Decimal> categoryExpense in expensesByCategory) {
                    double expenseValue = Decimal::ToDouble(categoryExpense.Value);
                    // Долю расходов переводим в угол сектора от 0 до 360°.
                    float sweepAngle =
                        (float)(expenseValue / totalExpenseValue * 360.0);
                    Color segmentColor = palette[colorIndex % palette->Length];
                    SolidBrush^ segmentBrush = gcnew SolidBrush(segmentColor);
                graphics->FillPie(segmentBrush, chartBounds, startAngle, sweepAngle);
                graphics->DrawPie(Pens::White, chartBounds, startAngle, sweepAngle);

                    graphics->FillRectangle(
                        segmentBrush, (float)legendX, (float)legendY, 14.0f, 14.0f);
                    graphics->DrawRectangle(Pens::Gray, legendX, legendY, 14, 14);
                    double percentage = expenseValue / totalExpenseValue * 100.0;
                    graphics->DrawString(
                        String::Format(L"{0} — {1} ({2:0.0}%)", categoryExpense.Key,
                            categoryExpense.Value.ToString(
                                "N2", CultureInfo::InvariantCulture),
                            percentage),
                        gcnew System::Drawing::Font("Segoe UI", 9), Brushes::Black,
                        (float)(legendX + 20), (float)legendY);

                    delete segmentBrush;
                    startAngle += sweepAngle;
                legendY += 22;
                colorIndex++;
            }
            }

        System::Void MyForm_FormClosing(System::Object^ sender, System::Windows::Forms::FormClosingEventArgs^ e) {
            // Все изменения сохраняются сразу после действия пользователя.
            // Повторная отправка всей базы при закрытии не требуется.
        }

            void RefreshAll() {
                RefreshGrid();
                RefreshBalance();
                RefreshAggregation();
                RefreshLimitsList();
                RefreshForecast();
                RefreshProfileSummary();
                if (tabControl->SelectedTab == tabStats) chartPanel->Invalidate();
            }

            void StyleControls(Control::ControlCollection^ controls) {
                Color pageColor = Color::FromArgb(244, 247, 251);
                Color cardColor = Color::White;
                Color accentColor = Color::FromArgb(45, 103, 210);
                Color textColor = Color::FromArgb(35, 45, 65);

                for each (Control^ control in controls) {
                    // Рекурсивно оформляем и вложенные элементы управления.
                    Button^ button = dynamic_cast<Button^>(control);
                    if (button != nullptr) {
                        button->FlatStyle = FlatStyle::Flat;
                    button->FlatAppearance->BorderSize = 0;
                        button->BackColor = accentColor;
                        button->ForeColor = Color::White;
                        button->Cursor = Cursors::Hand;
                        button->Font = gcnew System::Drawing::Font(
                            "Segoe UI Semibold", 9, FontStyle::Bold);
                    }

                    GroupBox^ groupBox = dynamic_cast<GroupBox^>(control);
                    if (groupBox != nullptr) {
                        groupBox->BackColor = cardColor;
                        groupBox->ForeColor = textColor;
                        groupBox->Font = gcnew System::Drawing::Font(
                            "Segoe UI Semibold", 9, FontStyle::Bold);
                    }

                    TabPage^ tabPage = dynamic_cast<TabPage^>(control);
                    if (tabPage != nullptr)
                        tabPage->BackColor = pageColor;

                    DataGridView^ table = dynamic_cast<DataGridView^>(control);
                    if (table != nullptr) {
                        table->BackgroundColor = cardColor;
                        table->BorderStyle = BorderStyle::None;
                        table->CellBorderStyle =
                            DataGridViewCellBorderStyle::SingleHorizontal;
                        table->GridColor = Color::FromArgb(225, 231, 240);
                        table->EnableHeadersVisualStyles = false;
                        table->ColumnHeadersDefaultCellStyle->BackColor =
                            Color::FromArgb(229, 236, 249);
                        table->ColumnHeadersDefaultCellStyle->ForeColor = textColor;
                        table->ColumnHeadersDefaultCellStyle->Font =
                            gcnew System::Drawing::Font(
                                "Segoe UI Semibold", 9, FontStyle::Bold);
                        table->ColumnHeadersHeight = 34;
                        table->DefaultCellStyle->SelectionBackColor =
                            Color::FromArgb(205, 220, 248);
                        table->DefaultCellStyle->SelectionForeColor = textColor;
                    table->RowTemplate->Height = 30;
                }

                    TextBox^ textBox = dynamic_cast<TextBox^>(control);
                    if (textBox != nullptr)
                        textBox->BorderStyle = BorderStyle::FixedSingle;

                    if (control->HasChildren)
                        StyleControls(control->Controls);
                }
            }

            void ApplyModernStyle() {
                this->BackColor = Color::FromArgb(244, 247, 251);
                this->Font = gcnew System::Drawing::Font("Segoe UI", 9);
                tabControl->Font = gcnew System::Drawing::Font(
                    "Segoe UI Semibold", 9, FontStyle::Bold);
                tabControl->Padding = Point(18, 6);
                StyleControls(this->Controls);

                Color dangerColor = Color::FromArgb(196, 66, 66);
                btnDelete->BackColor = dangerColor;
                btnRemoveLimit->BackColor = dangerColor;
                btnDeleteRecurring->BackColor = dangerColor;
                lblGreeting->ForeColor = Color::FromArgb(45, 103, 210);
                lblGoalProgress->ForeColor = Color::FromArgb(45, 103, 210);
            }

            // ==============================================================
            // Построение интерфейса
            // ==============================================================
            void InitializeComponent(void) {
                // Интерфейс создаётся программно. Здесь задаются размеры,
                // подписи и связи кнопок с обработчиками событий.
                this->components = nullptr;
                this->SuspendLayout();

                // ---- форма ----
                this->Text = L"Финансовый помощник";
                this->ShowIcon = false;
            this->ClientSize = System::Drawing::Size(992, 680);
            this->StartPosition = FormStartPosition::CenterScreen;
            this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
            this->MaximizeBox = false;
                this->Font = gcnew System::Drawing::Font("Segoe UI", 9);
                this->FormClosing += gcnew FormClosingEventHandler(this, &MyForm::MyForm_FormClosing);

                // ---- вкладки ----
                tabControl = gcnew TabControl();
                tabControl->Dock = DockStyle::Fill;
                tabOperations = gcnew TabPage(L"Операции");
                tabProfile    = gcnew TabPage(L"Личный профиль");
                tabCategories = gcnew TabPage(L"Категории и лимиты");
                tabRecurring   = gcnew TabPage(L"Регулярные платежи");
                tabStats      = gcnew TabPage(L"Статистика");
                tabOperations->AutoScroll = true;
                tabProfile->AutoScroll = true;
                tabCategories->AutoScroll = true;
                tabRecurring->AutoScroll = true;
                tabStats->AutoScroll = true;
                tabControl->Controls->Add(tabOperations);
                tabControl->Controls->Add(tabProfile);
                tabControl->Controls->Add(tabCategories);
                tabControl->Controls->Add(tabRecurring);
                tabControl->Controls->Add(tabStats);
                tabControl->SelectedIndexChanged += gcnew System::EventHandler(this, &MyForm::tabControl_SelectedIndexChanged);

                // ============ вкладка "Операции" ============
                grpNew = gcnew GroupBox();
                grpNew->Text = L"Новая операция";
            grpNew->Location = Point(8, 8);
            grpNew->Size = System::Drawing::Size(540, 170);

                lblType = gcnew Label();
                lblType->Text = L"Тип:";
                lblType->Location = Point(15, 30);
                lblType->Size = System::Drawing::Size(90, 20);
                cmbType = gcnew ComboBox();
                cmbType->DropDownStyle = ComboBoxStyle::DropDownList;
                cmbType->Location = Point(110, 27);
                cmbType->Size = System::Drawing::Size(150, 24);
                cmbType->Items->Add(L"Доход");
                cmbType->Items->Add(L"Расход");
                cmbType->SelectedIndex = 1;

                lblAmount = gcnew Label();
                lblAmount->Text = L"Сумма:";
                lblAmount->Location = Point(280, 30);
                lblAmount->Size = System::Drawing::Size(60, 20);
                txtAmount = gcnew TextBox();
                txtAmount->Location = Point(350, 27);
                txtAmount->Size = System::Drawing::Size(170, 24);

                lblCategory = gcnew Label();
                lblCategory->Text = L"Категория:";
                lblCategory->Location = Point(15, 66);
                lblCategory->Size = System::Drawing::Size(90, 20);
                cmbCategory = gcnew ComboBox();
                cmbCategory->DropDownStyle = ComboBoxStyle::DropDownList;
                cmbCategory->Location = Point(110, 63);
                cmbCategory->Size = System::Drawing::Size(150, 24);

                lblDate = gcnew Label();
                lblDate->Text = L"Дата:";
                lblDate->Location = Point(280, 66);
                lblDate->Size = System::Drawing::Size(60, 20);
                dtpDate = gcnew DateTimePicker();
                dtpDate->Format = DateTimePickerFormat::Short;
                dtpDate->Location = Point(350, 63);
                dtpDate->Size = System::Drawing::Size(170, 24);

                lblComment = gcnew Label();
                lblComment->Text = L"Комментарий:";
                lblComment->Location = Point(15, 102);
                lblComment->Size = System::Drawing::Size(95, 20);
                txtComment = gcnew TextBox();
                txtComment->Location = Point(110, 99);
                txtComment->Size = System::Drawing::Size(410, 24);

                btnAdd = gcnew Button();
                btnAdd->Text = L"Добавить";
                btnAdd->Location = Point(110, 132);
                btnAdd->Size = System::Drawing::Size(170, 28);
                btnAdd->Click += gcnew System::EventHandler(this, &MyForm::btnAdd_Click);

                grpNew->Controls->Add(lblType);     grpNew->Controls->Add(cmbType);
                grpNew->Controls->Add(lblAmount);   grpNew->Controls->Add(txtAmount);
                grpNew->Controls->Add(lblCategory); grpNew->Controls->Add(cmbCategory);
                grpNew->Controls->Add(lblDate);     grpNew->Controls->Add(dtpDate);
                grpNew->Controls->Add(lblComment);  grpNew->Controls->Add(txtComment);
                grpNew->Controls->Add(btnAdd);

                grpFilter = gcnew GroupBox();
                grpFilter->Text = L"Фильтр по периоду";
            grpFilter->Location = Point(556, 8);
            grpFilter->Size = System::Drawing::Size(420, 110);

                lblFrom = gcnew Label();
                lblFrom->Text = L"С:";
                lblFrom->Location = Point(15, 33);
                lblFrom->Size = System::Drawing::Size(25, 20);
                dtpFrom = gcnew DateTimePicker();
                dtpFrom->Format = DateTimePickerFormat::Short;
                dtpFrom->Location = Point(45, 30);
                dtpFrom->Size = System::Drawing::Size(150, 24);
                dtpFrom->Value = DateTime::Today.AddMonths(-1);

                lblTo = gcnew Label();
                lblTo->Text = L"По:";
                lblTo->Location = Point(210, 33);
                lblTo->Size = System::Drawing::Size(30, 20);
                dtpTo = gcnew DateTimePicker();
                dtpTo->Format = DateTimePickerFormat::Short;
                dtpTo->Location = Point(245, 30);
                dtpTo->Size = System::Drawing::Size(150, 24);

                btnApplyFilter = gcnew Button();
                btnApplyFilter->Text = L"Применить фильтр";
                btnApplyFilter->Location = Point(45, 66);
                btnApplyFilter->Size = System::Drawing::Size(150, 28);
                btnApplyFilter->Click += gcnew System::EventHandler(this, &MyForm::btnApplyFilter_Click);
                btnResetFilter = gcnew Button();
                btnResetFilter->Text = L"Показать все";
                btnResetFilter->Location = Point(245, 66);
                btnResetFilter->Size = System::Drawing::Size(150, 28);
                btnResetFilter->Click += gcnew System::EventHandler(this, &MyForm::btnResetFilter_Click);

                grpFilter->Controls->Add(lblFrom); grpFilter->Controls->Add(dtpFrom);
                grpFilter->Controls->Add(lblTo);   grpFilter->Controls->Add(dtpTo);
                grpFilter->Controls->Add(btnApplyFilter);
                grpFilter->Controls->Add(btnResetFilter);

                lblGreeting = gcnew Label();
                lblGreeting->Location = Point(556, 122);
            lblGreeting->Size = System::Drawing::Size(420, 24);
                lblGreeting->Font =
                    gcnew System::Drawing::Font("Segoe UI", 10, FontStyle::Bold);
                lblGreeting->Text = L"Здравствуйте!";

                lblBalance = gcnew Label();
            lblBalance->Location = Point(556, 144);
            lblBalance->Size = System::Drawing::Size(420, 38);
                lblBalance->Font = gcnew System::Drawing::Font("Segoe UI", 11, FontStyle::Bold);
                lblBalance->TextAlign = ContentAlignment::MiddleLeft;
                lblBalance->Text = L"Баланс: 0,00";

                lblStorage = gcnew Label();
            lblStorage->Location = Point(450, 190);
            lblStorage->Size = System::Drawing::Size(518, 24);
                lblStorage->TextAlign = ContentAlignment::MiddleRight;
                lblStorage->ForeColor = Color::DimGray;
                lblStorage->Text = L"Хранилище данных: текстовый файл";

                btnEdit = gcnew Button();
                btnEdit->Text = L"Редактировать выбранное";
                btnEdit->Location = Point(8, 186);
            btnEdit->Size = System::Drawing::Size(210, 28);
                btnEdit->Click += gcnew System::EventHandler(this, &MyForm::btnEdit_Click);
                btnDelete = gcnew Button();
                btnDelete->Text = L"Удалить выбранное";
                btnDelete->Location = Point(226, 186);
            btnDelete->Size = System::Drawing::Size(210, 28);
                btnDelete->Click += gcnew System::EventHandler(this, &MyForm::btnDelete_Click);

                grid = gcnew DataGridView();
            grid->Location = Point(8, 224);
            grid->Size = System::Drawing::Size(960, 410);
                grid->ReadOnly = true;
                grid->AllowUserToAddRows = false;
                grid->AllowUserToDeleteRows = false;
                grid->RowHeadersVisible = false;
                grid->SelectionMode = DataGridViewSelectionMode::FullRowSelect;
                grid->MultiSelect = true;
                grid->AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode::None;
                grid->Columns->Add("colType", L"Тип");
                grid->Columns->Add("colDate", L"Дата");
                grid->Columns->Add("colAmount", L"Сумма");
                grid->Columns->Add("colCategory", L"Категория");
                grid->Columns->Add("colComment", L"Комментарий");
                grid->Columns[0]->Width = 90;
                grid->Columns[1]->Width = 100;
                grid->Columns[2]->Width = 130;
                grid->Columns[2]->DefaultCellStyle->Alignment = DataGridViewContentAlignment::MiddleRight;
                grid->Columns[3]->Width = 180;
                grid->Columns[4]->AutoSizeMode = DataGridViewAutoSizeColumnMode::Fill;

                tabOperations->Controls->Add(grpNew);
                tabOperations->Controls->Add(grpFilter);
                tabOperations->Controls->Add(lblGreeting);
                tabOperations->Controls->Add(lblBalance);
                tabOperations->Controls->Add(lblStorage);
                tabOperations->Controls->Add(btnEdit);
                tabOperations->Controls->Add(btnDelete);
                tabOperations->Controls->Add(grid);

                // ============ вкладка "Личный профиль" ============
                GroupBox^ profileCard = gcnew GroupBox();
                profileCard->Text = L"Основная информация";
            profileCard->Location = Point(90, 45);
            profileCard->Size = System::Drawing::Size(790, 430);

                Label^ profileTitle = gcnew Label();
                profileTitle->Text = L"Настройте приложение под себя";
                profileTitle->Location = Point(32, 35);
                profileTitle->Size = System::Drawing::Size(710, 35);
                profileTitle->Font =
                    gcnew System::Drawing::Font("Segoe UI Semibold", 16, FontStyle::Bold);

                Label^ profileNameLabel = gcnew Label();
                profileNameLabel->Text = L"Имя";
                profileNameLabel->Location = Point(35, 95);
                profileNameLabel->Size = System::Drawing::Size(150, 22);
                txtProfileName = gcnew TextBox();
                txtProfileName->Location = Point(210, 92);
                txtProfileName->Size = System::Drawing::Size(250, 26);

                Label^ profileCurrencyLabel = gcnew Label();
                profileCurrencyLabel->Text = L"Основная валюта";
                profileCurrencyLabel->Location = Point(35, 140);
                profileCurrencyLabel->Size = System::Drawing::Size(150, 22);
                cmbProfileCurrency = gcnew ComboBox();
                cmbProfileCurrency->DropDownStyle = ComboBoxStyle::DropDownList;
                cmbProfileCurrency->Location = Point(210, 137);
                cmbProfileCurrency->Size = System::Drawing::Size(250, 26);
                cmbProfileCurrency->Items->AddRange(
                    gcnew array<Object^> { L"BYN", L"RUB", L"USD", L"EUR" });

                Label^ monthlyBudgetLabel = gcnew Label();
                monthlyBudgetLabel->Text = L"Бюджет на месяц";
                monthlyBudgetLabel->Location = Point(35, 185);
                monthlyBudgetLabel->Size = System::Drawing::Size(150, 22);
                txtMonthlyBudget = gcnew TextBox();
                txtMonthlyBudget->Location = Point(210, 182);
                txtMonthlyBudget->Size = System::Drawing::Size(250, 26);

                Label^ financialGoalLabel = gcnew Label();
                financialGoalLabel->Text = L"Финансовая цель";
                financialGoalLabel->Location = Point(35, 230);
                financialGoalLabel->Size = System::Drawing::Size(150, 22);
                txtFinancialGoal = gcnew TextBox();
                txtFinancialGoal->Location = Point(210, 227);
            txtFinancialGoal->Size = System::Drawing::Size(500, 26);

                Label^ goalAmountLabel = gcnew Label();
                goalAmountLabel->Text = L"Необходимая сумма";
                goalAmountLabel->Location = Point(35, 275);
                goalAmountLabel->Size = System::Drawing::Size(150, 22);
                txtGoalAmount = gcnew TextBox();
                txtGoalAmount->Location = Point(210, 272);
                txtGoalAmount->Size = System::Drawing::Size(250, 26);

                lblGoalProgress = gcnew Label();
                lblGoalProgress->Location = Point(35, 320);
            lblGoalProgress->Size = System::Drawing::Size(675, 32);
                lblGoalProgress->Font =
                    gcnew System::Drawing::Font("Segoe UI", 10, FontStyle::Bold);

                btnSaveProfile = gcnew Button();
                btnSaveProfile->Text = L"Сохранить профиль";
            btnSaveProfile->Location = Point(530, 365);
            btnSaveProfile->Size = System::Drawing::Size(180, 36);
                btnSaveProfile->Click += gcnew System::EventHandler(
                    this, &MyForm::btnSaveProfile_Click);

                profileCard->Controls->Add(profileTitle);
                profileCard->Controls->Add(profileNameLabel);
                profileCard->Controls->Add(txtProfileName);
                profileCard->Controls->Add(profileCurrencyLabel);
                profileCard->Controls->Add(cmbProfileCurrency);
                profileCard->Controls->Add(monthlyBudgetLabel);
                profileCard->Controls->Add(txtMonthlyBudget);
                profileCard->Controls->Add(financialGoalLabel);
                profileCard->Controls->Add(txtFinancialGoal);
                profileCard->Controls->Add(goalAmountLabel);
                profileCard->Controls->Add(txtGoalAmount);
                profileCard->Controls->Add(lblGoalProgress);
                profileCard->Controls->Add(btnSaveProfile);

                GroupBox^ storageCard = gcnew GroupBox();
                storageCard->Text = L"Где хранятся данные";
            storageCard->Location = Point(90, 500);
            storageCard->Size = System::Drawing::Size(790, 110);
                lblStorageDetails = gcnew Label();
                lblStorageDetails->Location = Point(25, 30);
            lblStorageDetails->Size = System::Drawing::Size(735, 60);
            lblStorageDetails->Text =
                L"Сейчас данные сохраняются в текстовый файл.";
                storageCard->Controls->Add(lblStorageDetails);

                tabProfile->Controls->Add(profileCard);
                tabProfile->Controls->Add(storageCard);

                // ============ вкладка "Категории и лимиты" ============
                grpCat = gcnew GroupBox();
                grpCat->Text = L"Категории";
            grpCat->Location = Point(8, 8);
            grpCat->Size = System::Drawing::Size(452, 600);

                lstCategories = gcnew ListBox();
            lstCategories->Location = Point(16, 28);
            lstCategories->Size = System::Drawing::Size(420, 460);

                lblNewCat = gcnew Label();
                lblNewCat->Text = L"Название категории:";
            lblNewCat->Location = Point(16, 500);
            lblNewCat->Size = System::Drawing::Size(420, 20);
            txtNewCategory = gcnew TextBox();
            txtNewCategory->Location = Point(16, 522);
            txtNewCategory->Size = System::Drawing::Size(420, 24);

                btnAddCategory = gcnew Button();
                btnAddCategory->Text = L"Добавить категорию";
            btnAddCategory->Location = Point(16, 554);
            btnAddCategory->Size = System::Drawing::Size(200, 32);
                btnAddCategory->Click += gcnew System::EventHandler(this, &MyForm::btnAddCategory_Click);
                btnRenameCategory = gcnew Button();
                btnRenameCategory->Text = L"Переименовать выбранную";
            btnRenameCategory->Location = Point(226, 554);
            btnRenameCategory->Size = System::Drawing::Size(210, 32);
                btnRenameCategory->Click += gcnew System::EventHandler(this, &MyForm::btnRenameCategory_Click);

                grpCat->Controls->Add(lstCategories);
                grpCat->Controls->Add(lblNewCat);
                grpCat->Controls->Add(txtNewCategory);
                grpCat->Controls->Add(btnAddCategory);
                grpCat->Controls->Add(btnRenameCategory);

                grpLimit = gcnew GroupBox();
                grpLimit->Text = L"Лимиты расходов по категориям";
            grpLimit->Location = Point(468, 8);
            grpLimit->Size = System::Drawing::Size(508, 600);

                lblLimCat = gcnew Label();
                lblLimCat->Text = L"Категория:";
                lblLimCat->Location = Point(16, 33);
                lblLimCat->Size = System::Drawing::Size(80, 20);
                cmbLimitCategory = gcnew ComboBox();
                cmbLimitCategory->DropDownStyle = ComboBoxStyle::DropDownList;
                cmbLimitCategory->Location = Point(100, 30);
                cmbLimitCategory->Size = System::Drawing::Size(220, 24);

                lblLimAmount = gcnew Label();
                lblLimAmount->Text = L"Лимит:";
                lblLimAmount->Location = Point(16, 69);
                lblLimAmount->Size = System::Drawing::Size(80, 20);
                txtLimitAmount = gcnew TextBox();
                txtLimitAmount->Location = Point(100, 66);
                txtLimitAmount->Size = System::Drawing::Size(220, 24);

                btnSetLimit = gcnew Button();
                btnSetLimit->Text = L"Установить лимит";
                btnSetLimit->Location = Point(16, 100);
                btnSetLimit->Size = System::Drawing::Size(220, 32);
                btnSetLimit->Click += gcnew System::EventHandler(this, &MyForm::btnSetLimit_Click);
                btnRemoveLimit = gcnew Button();
                btnRemoveLimit->Text = L"Удалить выбранный лимит";
                btnRemoveLimit->Location = Point(246, 100);
                btnRemoveLimit->Size = System::Drawing::Size(246, 32);
                btnRemoveLimit->Click += gcnew System::EventHandler(this, &MyForm::btnRemoveLimit_Click);

                gridLimits = gcnew DataGridView();
            gridLimits->Location = Point(16, 144);
            gridLimits->Size = System::Drawing::Size(476, 440);
                gridLimits->ReadOnly = true;
                gridLimits->AllowUserToAddRows = false;
                gridLimits->AllowUserToDeleteRows = false;
                gridLimits->RowHeadersVisible = false;
                gridLimits->SelectionMode = DataGridViewSelectionMode::FullRowSelect;
                gridLimits->MultiSelect = false;
                gridLimits->AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode::None;
                gridLimits->Columns->Add("lcCat", L"Категория");
                gridLimits->Columns->Add("lcLimit", L"Лимит");
                gridLimits->Columns->Add("lcSpent", L"Потрачено");
                gridLimits->Columns->Add("lcLeft", L"Осталось");
                gridLimits->Columns[0]->Width = 170;
                gridLimits->Columns[1]->Width = 100;
                gridLimits->Columns[2]->Width = 100;
                gridLimits->Columns[3]->AutoSizeMode = DataGridViewAutoSizeColumnMode::Fill;
                gridLimits->Columns[1]->DefaultCellStyle->Alignment = DataGridViewContentAlignment::MiddleRight;
                gridLimits->Columns[2]->DefaultCellStyle->Alignment = DataGridViewContentAlignment::MiddleRight;
                gridLimits->Columns[3]->DefaultCellStyle->Alignment = DataGridViewContentAlignment::MiddleRight;

                grpLimit->Controls->Add(lblLimCat);    grpLimit->Controls->Add(cmbLimitCategory);
                grpLimit->Controls->Add(lblLimAmount); grpLimit->Controls->Add(txtLimitAmount);
                grpLimit->Controls->Add(btnSetLimit);  grpLimit->Controls->Add(btnRemoveLimit);
                grpLimit->Controls->Add(gridLimits);

                tabCategories->Controls->Add(grpCat);
                tabCategories->Controls->Add(grpLimit);

                // ============ вкладка "Регулярные платежи" ============
                GroupBox^ recurringEditor = gcnew GroupBox();
                recurringEditor->Text = L"Новый ежемесячный платёж";
            recurringEditor->Location = Point(8, 8);
            recurringEditor->Size = System::Drawing::Size(960, 150);

                Label^ recurringTypeLabel = gcnew Label();
                recurringTypeLabel->Text = L"Тип:";
                recurringTypeLabel->Location = Point(16, 32);
                recurringTypeLabel->Size = System::Drawing::Size(80, 20);
                cmbRecurringType = gcnew ComboBox();
                cmbRecurringType->DropDownStyle = ComboBoxStyle::DropDownList;
                cmbRecurringType->Location = Point(100, 29);
                cmbRecurringType->Size = System::Drawing::Size(150, 24);
                cmbRecurringType->Items->Add(L"Доход");
                cmbRecurringType->Items->Add(L"Расход");
                cmbRecurringType->SelectedIndex = 1;

                Label^ recurringAmountLabel = gcnew Label();
                recurringAmountLabel->Text = L"Сумма:";
                recurringAmountLabel->Location = Point(280, 32);
                recurringAmountLabel->Size = System::Drawing::Size(70, 20);
                txtRecurringAmount = gcnew TextBox();
                txtRecurringAmount->Location = Point(350, 29);
                txtRecurringAmount->Size = System::Drawing::Size(150, 24);

                Label^ recurringCategoryLabel = gcnew Label();
                recurringCategoryLabel->Text = L"Категория:";
                recurringCategoryLabel->Location = Point(530, 32);
                recurringCategoryLabel->Size = System::Drawing::Size(80, 20);
                cmbRecurringCategory = gcnew ComboBox();
                cmbRecurringCategory->DropDownStyle = ComboBoxStyle::DropDownList;
                cmbRecurringCategory->Location = Point(615, 29);
                cmbRecurringCategory->Size = System::Drawing::Size(200, 24);

                Label^ recurringDayLabel = gcnew Label();
                recurringDayLabel->Text = L"День месяца:";
                recurringDayLabel->Location = Point(16, 72);
                recurringDayLabel->Size = System::Drawing::Size(90, 20);
                numRecurringDay = gcnew NumericUpDown();
                numRecurringDay->Minimum = 1;
                numRecurringDay->Maximum = 31;
                numRecurringDay->Value = Math::Min(DateTime::Today.Day, 28);
                numRecurringDay->Location = Point(110, 69);
                numRecurringDay->Size = System::Drawing::Size(70, 24);

                Label^ recurringCommentLabel = gcnew Label();
                recurringCommentLabel->Text = L"Комментарий:";
                recurringCommentLabel->Location = Point(210, 72);
                recurringCommentLabel->Size = System::Drawing::Size(95, 20);
                txtRecurringComment = gcnew TextBox();
                txtRecurringComment->Location = Point(305, 69);
                txtRecurringComment->Size = System::Drawing::Size(510, 24);

                btnAddRecurring = gcnew Button();
                btnAddRecurring->Text = L"Добавить регулярный платёж";
            btnAddRecurring->Location = Point(615, 105);
            btnAddRecurring->Size = System::Drawing::Size(200, 30);
                btnAddRecurring->Click += gcnew System::EventHandler(
                    this, &MyForm::btnAddRecurring_Click);

                recurringEditor->Controls->Add(recurringTypeLabel);
                recurringEditor->Controls->Add(cmbRecurringType);
                recurringEditor->Controls->Add(recurringAmountLabel);
                recurringEditor->Controls->Add(txtRecurringAmount);
                recurringEditor->Controls->Add(recurringCategoryLabel);
                recurringEditor->Controls->Add(cmbRecurringCategory);
                recurringEditor->Controls->Add(recurringDayLabel);
                recurringEditor->Controls->Add(numRecurringDay);
                recurringEditor->Controls->Add(recurringCommentLabel);
                recurringEditor->Controls->Add(txtRecurringComment);
                recurringEditor->Controls->Add(btnAddRecurring);

                btnDeleteRecurring = gcnew Button();
                btnDeleteRecurring->Text = L"Удалить выбранный";
            btnDeleteRecurring->Location = Point(8, 166);
                btnDeleteRecurring->Size = System::Drawing::Size(190, 30);
                btnDeleteRecurring->Click += gcnew System::EventHandler(
                    this, &MyForm::btnDeleteRecurring_Click);

                gridRecurring = gcnew DataGridView();
            gridRecurring->Location = Point(8, 204);
            gridRecurring->Size = System::Drawing::Size(960, 430);
                gridRecurring->ReadOnly = true;
                gridRecurring->AllowUserToAddRows = false;
                gridRecurring->AllowUserToDeleteRows = false;
                gridRecurring->RowHeadersVisible = false;
                gridRecurring->SelectionMode = DataGridViewSelectionMode::FullRowSelect;
                gridRecurring->MultiSelect = false;
                gridRecurring->Columns->Add("rpType", L"Тип");
                gridRecurring->Columns->Add("rpAmount", L"Сумма");
                gridRecurring->Columns->Add("rpCategory", L"Категория");
                gridRecurring->Columns->Add("rpDay", L"День");
                gridRecurring->Columns->Add("rpNextDate", L"Следующая дата");
                gridRecurring->Columns->Add("rpComment", L"Комментарий");
                gridRecurring->Columns[0]->Width = 85;
                gridRecurring->Columns[1]->Width = 110;
                gridRecurring->Columns[2]->Width = 170;
                gridRecurring->Columns[3]->Width = 70;
                gridRecurring->Columns[4]->Width = 130;
                gridRecurring->Columns[5]->AutoSizeMode =
                    DataGridViewAutoSizeColumnMode::Fill;

                tabRecurring->Controls->Add(recurringEditor);
                tabRecurring->Controls->Add(btnDeleteRecurring);
                tabRecurring->Controls->Add(gridRecurring);

                // ============ вкладка "Статистика" ============
                gridAgg = gcnew DataGridView();
            gridAgg->Location = Point(8, 8);
            gridAgg->Size = System::Drawing::Size(430, 632);
                gridAgg->ReadOnly = true;
                gridAgg->AllowUserToAddRows = false;
                gridAgg->AllowUserToDeleteRows = false;
                gridAgg->RowHeadersVisible = false;
                gridAgg->SelectionMode = DataGridViewSelectionMode::FullRowSelect;
                gridAgg->AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode::None;
                gridAgg->Columns->Add("aCat", L"Категория");
                gridAgg->Columns->Add("aInc", L"Доходы");
                gridAgg->Columns->Add("aExp", L"Расходы");
                gridAgg->Columns[0]->AutoSizeMode = DataGridViewAutoSizeColumnMode::Fill;
                gridAgg->Columns[1]->Width = 120;
                gridAgg->Columns[2]->Width = 120;
                gridAgg->Columns[1]->DefaultCellStyle->Alignment = DataGridViewContentAlignment::MiddleRight;
                gridAgg->Columns[2]->DefaultCellStyle->Alignment = DataGridViewContentAlignment::MiddleRight;

                lblChartTitle = gcnew Label();
                lblForecast = gcnew Label();
            lblForecast->Location = Point(446, 8);
            lblForecast->Size = System::Drawing::Size(530, 70);
                lblForecast->Font =
                    gcnew System::Drawing::Font("Segoe UI", 10, FontStyle::Bold);
                lblForecast->Text = L"Прогноз рассчитывается...";

                btnExportReport = gcnew Button();
                btnExportReport->Text = L"Экспортировать отчёт в TXT";
            btnExportReport->Location = Point(446, 84);
                btnExportReport->Size = System::Drawing::Size(220, 32);
                btnExportReport->Click += gcnew System::EventHandler(
                    this, &MyForm::btnExportReport_Click);

                lblChartTitle->Text = L"Структура расходов по категориям";
            lblChartTitle->Location = Point(446, 126);
            lblChartTitle->Size = System::Drawing::Size(530, 24);
                lblChartTitle->Font = gcnew System::Drawing::Font("Segoe UI", 11, FontStyle::Bold);

                chartPanel = gcnew Panel();
            chartPanel->Location = Point(446, 156);
            chartPanel->Size = System::Drawing::Size(530, 484);
                chartPanel->BackColor = Color::White;
                chartPanel->BorderStyle = System::Windows::Forms::BorderStyle::FixedSingle;
                chartPanel->Paint += gcnew PaintEventHandler(this, &MyForm::chartPanel_Paint);

                tabStats->Controls->Add(gridAgg);
                tabStats->Controls->Add(lblForecast);
                tabStats->Controls->Add(btnExportReport);
                tabStats->Controls->Add(lblChartTitle);
                tabStats->Controls->Add(chartPanel);

                // ---- финал ----
                this->Controls->Add(tabControl);
                this->ResumeLayout(false);
            }
        };
    }
