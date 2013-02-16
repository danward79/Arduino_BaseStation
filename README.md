Arduino_BaseStation
===================

RFM12B Sensor BaseStation, collects data from my external weather sensor and power sensor.

Data is fed to Cosm for basic graphing and visualisation, the weather data is also tweeted periodically.

In addition on a periodic basis the current time is updated from an NTP server and broadcast, so that any sensors that need time can update.

There is also provision for a basic webpage to allow control of my Aquarium Light which I have yet to build.

Uses librarys from Jeelibs for RFM12B and DS1307. Also code for NTP stolen and modified from someone else who pinched some of it as well I think!

Have Fun!
