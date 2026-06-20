# Production Test Issues

Format: zagadnienie + krótko rozwiązanie.

## 1. Days Left liczone z aktywnego configu, nie z pending

Days Left na GUI pokazuje wartość niezgodną z resztą pól (Daily Dose, Weekly, Pump Time), gdy kanał ma pending zmiany — bo backend liczy ją z `_activeConfig`, a pozostałe pola z `pending` configu.

**Rozwiązanie:** `getDaysRemaining()` ma używać tej samej config (pending jeśli istnieje), co reszta pól w `/api/status`.

*Status: niewykonane.*

## 2. Brak przycisku do natychmiastowego zatwierdzenia pending configu

Save zapisuje config jako pending — wchodzi w życie dopiero o północy. Potrzebny był przycisk działający jak komenda CLI ('n'), ale ograniczony do jednego kanału: zatwierdza pending od razu i czyści historię trwającej doby tego kanału.

**Rozwiązanie:** 4. przycisk "Apply Pending" na karcie kanału (PIN-lock) → `ChannelManager::resetDailyState(channel)` + endpoint `/api/apply-pending`, który robi `applyPendingChanges(channel)` + reset stanu dobowego tylko tego kanału. Mobile: przyciski w układzie 2x2.

*Status: wykonane (commit c03b7f4).*
