#pragma once

namespace BudgetTracker {

    using namespace System;
    using namespace System::Globalization;

    // Единое место для подготовки и проверки пользовательского ввода.
    // Благодаря этому обработчики кнопок не повторяют одинаковые проверки.
    public ref class InputValidator abstract sealed {
    private:
        static String^ NormalizeNumber(String^ text) {
            // Пользователь может вводить дробную часть и через точку,
            // и через запятую. Для разбора приводим оба варианта к одному виду.
            return text->Trim()->Replace(" ", "")->Replace(",", ".");
        }

    public:
        static bool TryParsePositiveAmount(String^ text, Decimal% amount) {
            return Decimal::TryParse(
                NormalizeNumber(text),
                NumberStyles::Any,
                CultureInfo::InvariantCulture,
                amount) && amount > Decimal::Zero;
        }

        static bool TryParseNonNegativeAmount(String^ text, Decimal% amount) {
            return Decimal::TryParse(
                NormalizeNumber(text),
                NumberStyles::Any,
                CultureInfo::InvariantCulture,
                amount) && amount >= Decimal::Zero;
        }

        static String^ NormalizeCategoryName(String^ text) {
            // Символ "|" заменяется, чтобы текст безопасно использовался
            // в отчётах и при передаче данных.
            return text->Trim()->Replace("|", "/");
        }

        static String^ NormalizeComment(String^ text) {
            return text->Replace("|", "/")->Replace("\r", " ")->Replace("\n", " ");
        }
    };
}
