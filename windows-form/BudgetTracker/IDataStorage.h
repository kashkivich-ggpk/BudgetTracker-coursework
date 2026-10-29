#pragma once

#include "DomainModels.h"

namespace BudgetTracker {

    using namespace System;

    // Общий договор для любого хранилища.
    // Форма умеет только загружать и сохранять BudgetData и не зависит
    // от того, используется Supabase, PostgreSQL или другое хранилище.
    public interface class IDataStorage {
        BudgetData^ Load();
        void Save(BudgetData^ data);

        property String^ Name {
            String^ get();
        }
    };
}
