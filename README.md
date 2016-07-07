 Batmon
========

I found myself in need of a smarter battery monitor on Linux, so I wrote and
tested this little guy during the weekend.

**Nananananananana**

 How it works
--------------

Batmon runs as a daemon, Batmon reads "threshold: command" pairs from
/etc/batmon.conf, Batmon execute commands only when system is running on
battery power.

 POW!
------

* Batmon only executes commands when it transitions from a battery level higher
  than *x* threshold to a value lower or equal to said threshold.

* Batmon only executes commands when it's running on battery power.

* Batmon is somewhat battery-concious and will try to sleep as much as
  possible.

* Batmon will wake up more frequently as battery power depletes.

* Batmon can be woken up by sending SIGHUP. Useful when combined with ACPI
  events.

