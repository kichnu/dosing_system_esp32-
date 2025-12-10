Przedstawię to w bardziej spójny sposób od strony danych na potrzeby tworzenia JSON'a wynikowego:
Zdefiniuję robocze zmienne (nadasz im później odpowiednie fachowe nazwy):

wartość czasu we wszystkich zmiennych jest wartością całkowitą, wartość minimalna to 1 sekunda (przykładowy zapis: 4s)

czujnik 1-> T_1
czujnik 2-> T_2
pompa dolewająca wodę-> PUMP
praca pompy-> PUMP_WORK 
moment startu pompy-> PUMP_START
czas pracy pompy-> PUMP_WORK_TIME
moment zatrzymania pompy-> PUMP_STOP

suma dolanej wody na 1 dobę-> FILL_WATER
maksymalna dolewka wody na 1 dobę-> FILL_WATER_MAX



czas od TRIGGER do załączenia PUMP-> TIME_TO_PUMP
TIME_TO_PUMP = TRYB_1_TIME

Uruchomienie systemu(włączenie zasilania-> START
Cały cykl pracy-> CYCLE

tryb pracy nr 1-> TRYB_1
tryb pracy nr 2-> TRYB_2

czas trwania TRYB_1-> TRYB_1_TIME
czas trwania TRYB_2-> TRYB_2_TIME
czas w którym system nic nie robi, oczekując na TRIGGER->IDLE_TIME
czas przeznaczony na logowanie danych-> LOGGING_TIME (może mieć stały zarezerwawany czas np 5s, który doda się do czasu CYCLE)

czujnik T_1 podczas niskiego stanu wody-> T_1-HIGH niski stan wody generuje stan włączony czujnika wody
czujnik T_2 podczas niskiego stanu wody-> T_2-HIGH niski stan wody generuje stan włączony czujnika wody
czujnik T_1 podczas wysokiego stanu wody-> T_1-LOW wysoki stan wody generuje stan wyłączony czujnika wody
czujnik T_2 podczas wysokiego stanu wody-> T_2-LOW wysoki stan wody generuje stan wyłączony czujnika wody

aktywacja T_1 OR T_2-> TRIGGER
funkcja porównująca czasy (np TIME_GAP_1 z THRESHOLD_1 według zasady:TIME_GAP_x < THRESHOLD_x zwraca {0}, a wprzeciwnym przypadku zwraca {1})-> SENSOR_TIME_MATCH_FUNCTION

różnica czasu w aktywacji pomiędzy czujnikami T_1 i T_2-> TIME_GAP_1
różnica czasu w deaktywacji pomiędzy czujnikami T_1 i T_2-> TIME_GAP_2
maksymalna wartość czasu TIME_GAP_1-> TIME_GAP_1_MAX
maksymalna wartość czasu TIME_GAP_2-> TIME_GAP_2_MAX

różnica czasu pomiędzy start PUMP a deaktywacją T_1 OR T_2-> WATER_TRIGGER_TIME
czas maksymalnego oczekiwania od PUMP_START do WATER_TRIGGER_TIME-> WATER_TRIGGER_MAX_TIME

wartość progowa czasu dla TIME_GAP_1 w którym funkcja go analizująca zwróci wartość 0 lub 1-> THRESHOLD_1
wartość progowa czasu dla TIME_GAP_2 w którym funkcja go analizująca zwróci wartość 0 lub 1-> THRESHOLD_2
wartość progowa czasu dla WATER_TRIGGER_TIME w którym funkcja go analizująca zwróci wartość 0 lub 1-> THRESHOLD_WATER



ALGORYTM SYSTEMU:

cykle pracy w TRYB_1 (woda opada, tryb zaczyna się od TRIGGER)-
w momencie zaistnienia TRIGGER system czeka na aktywację drugiego czujnika mierząć TIME_GAP_1 przez czas TIME_GAP_1_MAX.
po zakończeniu TIME_GAP_1_MAX funkcja SENSOR_TIME_MATCH_FUNCTION zwróci {0 lub 1} porównując TIME_GAP_1 z THRESHOLD_1.
po czasie TIME_TO_PUMP system załączy PUMP na czas PUMP_WORK_TIME.
cykl TRYB_1 zakończy się w momencie PUMP_START.

(zależności czasowe nadzorowane przez 'static_assert' podczas kompilacji)
TIME_TO_PUMP > 300s AND TIME_TO_PUMP < 600s
TIME_TO_PUMP > (TIME_GAP_1_MAX + 10s)
TIME_TO_PUMP > (THRESHOLD_1 + 30s)


Cykle pracy w TRYB_2 (woda się podnosi, tryb zaczyna się od PUMP_START)-
jeżeli WATER_TRIGGER_TIME > WATER_TRIGGER_MAX_TIME system natychmiast ponownie uruchamia PUMP_WORK na czas PUMP_WORK_TIME, jeżeli nadal WATER_TRIGGER_TIME > WATER_TRIGGER_MAX_TIME system natychmiast uruchamia PUMP_WORK na czas PUMP_WORK_TIME, jeżeli nadal WATER_TRIGGER_TIME > WATER_TRIGGER_MAX_TIME, system natychmiast przerywa TRYB_2 i jednocześnie cały CYCLE. System jednocześnie wystawi sygnał na pinie sterownika i będzie oczekiwał na fizyczny reset na innym pinie sterownika. Po resecie sterownika system przechodzi w IDLE_TIME i oczekuje na TRIGGER.

jeżeli za pierwszym, drugim lub trzecim razem zostanie spełniony warunek: WATER_TRIGGER_TIME <= WATER_TRIGGER_MAX_TIME, wtedy system sprawdzi wynik z SENSOR_TIME_MATCH_FUNCTION z TRYB_1 i jeżeli = 0 będzie czekał na deaktywację drugiego czujnika mierząc TIME_GAP_2.
na podstawie TIME_GAP_2 funkcja SENSOR_TIME_MATCH_FUNCTION zwróci {0} lub {1}.
jeżeli wynik z SENSOR_TIME_MATCH_FUNCTION z TRYB_1 = 1,  to system nie będzie czekał na TIME_GAP_2 i funkcja SENSOR_TIME_MATCH_FUNCTION zwróci automatycznie {0} by nie dublować dwóch wartości {1}
jeżeli zostanie spełniony warunek: WATER_TRIGGER_TIME <= WATER_TRIGGER_MAX_TIME AND funkcja SENSOR_TIME_MATCH_FUNCTION zróci wartość {1} lub {0} AND dla PUMP zakończył się PUMP_WORK_TIME, to system przejdzie w LOGGING_TIME lub IDLE_TIME.

(zależności czasowe nadzorowane przez 'static_assert' podczas kompilacji)
WATER_TRIGGER_MAX_TIME > PUMP_WORK_TIME

w momencie przejścia w IDLE_TIME system wykona operację logowania do bazy danych na zdalnym serwerze oraz do pamięci FRAM (Adafruit) Można te operacje zarezerwować czas opóźniając przejście w tryb IDLE_TIME.


#############Logowanie i zapis danych#########
Część JSON który zostanie zapisany w bazie danych i wyświetlonyna www -> XX-YY-ZZ-VVVV gdzie:
XX - suma rezultatów z funkcji SENSOR_TIME_MATCH_FUNCTION które zwróci dla TIME_GAP_1 z ostatnich 2 tygodni
YY - suma rezultatów z funkcji SENSOR_TIME_MATCH_FUNCTION które zwróci dla TIME_GAP_2 z ostatnich 2 tygodni
zz - suma rezultatów z funkcji SENSOR_TIME_MATCH_FUNCTION które zwróci dla WATER_TRIGGER_TIME z ostatnich 2 tygodni
W powyższych trzech wartościach maksymalny zakres to 00 do 99, jeżeli więcej to wyświetli 'OV', jeżeli błędy to 'ER' Zaproponuj jakie błędy mogą wystąpić - do mojej decyzji.
VVVV - FILL_WATER: liczba PUMP_WORK * objętość 1 dolewki(zaimplementowana w istniejącym kodzie) z ostatnich 24 godzin. Zakres 0000 do 9999, jeżeli przekroczy to 'OVER', jeżeli bład to 'ERR.' Zaproponuj jakie błędy mogą wystąpić - do mojej decyzji.
Na etapie logowania błędów sprawdzana jest czy: FILL_WATER < FILL_WATER_MAX, jeżeli true, to System jednocześnie wystawi sygnał na pinie sterownika i będzie oczekiwał na fizyczny reset na innym pinie sterownika. Po resecie sterownika system przechodzi w IDLE_TIME i oczekuje na TRIGGER. Te same piny co w przypadku: WATER_TRIGGER_TIME > WATER_TRIGGER_MAX_TIME. Ważne: najpierw system zaloguje dane zapisując w tedy w miejsce 'VVVV' wartość 'OVER' i dopiero później przejdzie do wystawienia sygnału na pin i oczekiwania na reset.

Teraz sprawdź logikę tego algorytmu, spójność i ewentualne 'dziury'. Nie pisz żadnego kodu, a jedynie oczekiwane info do mojej decyzji.
Zadaj pytania o brakujące informacje. W momencie pisania kodu otrzymasz już istniejący kod do doimplementowania tego algorytmu do całego projektu.













