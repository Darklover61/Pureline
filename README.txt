+------------------------------------------------------------+
|                * Kaptan Yosun's Pureline *                 |
+------------------------------------------------------------+

+--------------------------------------------+---------------+
| General Information                        | Version       |
+--------------------------------------------+---------------+
| Base                                       | [Mainline]    |
| DirectX                                    | [8]           |
| Granny                                     | [2.11.8]      |
+--------------------------------------------+---------------+

+------------------------------+---------+--------+----------+
| Extern Information           | Server  | Client | Version  |
|------------------------------+---------+--------+----------+
| MiniLZO                      | YES     | NO     | [2.10]   |
| Boost                        | YES     | YES    | [1.88.0] |
| CryptoPP                     | YES     | YES    | [8.9.0]  |
| SpeedTree                    | NO      | YES    | [1.6.0]  |
+------------------------------+---------+--------+----------+

+-----------------------------------+--------+--------+------+
| New Code                          | Server | Client | Pack |
+-----------------------------------+--------+--------+------+
|                                   |        |        |      |
+-----------------------------------+--------+--------+------+

+-----------------------------------+--------+--------+------+
| New Code [Small]                  | Server | Client | Pack |
+-----------------------------------+--------+--------+------+
|                                   |        |        |      |
+-----------------------------------+--------+--------+------+

+-----------------------------------+--------+--------+------+
| Removed Systems/Significant Code  | Server | Client | Pack |
+-----------------------------------+--------+--------+------+
| Xtrap                             | YES    | YES    | NO   |
| Hackshield                        | YES    | YES    | NO   |
| PC_Bang            [+ Dump Proto] | YES    | YES    | YES  |
| Lottery            [+ Dump Proto] | YES    | YES    | YES  |
| Teen                              | YES    | NO     | NO   |
| nProtect_GameGuard                | NO     | YES    | NO   |
| Software Tiling                   | NO     | YES    | YES  |
| libserverkey                      | YES    | NO     | NO   |
| BlockCountryIp                    | YES    | NO     | NO   |
| Mobile - SMS                      | YES    | YES    | YES  |
|                                   |        |        |      |
+-----------------------------------+--------+--------+------+

+-----------------------------------+--------+--------+------+
| Removed Small/Insignificant Code  | Server | Client | Pack |
+-----------------------------------+--------+--------+------+
| Netmarble                         | YES    | NO     | NO   |
| HammerOfTor                       | YES    | NO     | NO   |
| Roulette                          | YES    | NO     | NO   |
|                                   |        |        |      |
+-----------------------------------+--------+--------+------+

+-------------------------------+----------------------------+
| New Defines                   | Location                   |
+-------------------------------+----------------------------+
|                               |                            |
+-------------------------------+----------------------------+

+------------------------------------------+-----------------+
| New Configurable Options                 | Location        |
+------------------------------------------+---------------- +
|                                          |                 |
+------------------------------------------+-----------------+

+------------------------------------------------------------+
|                       * Bug Fixes *                        |
+------------------------------------------------------------+
(TODO) [REVERSED] FOG_FIX          : Official fog mode fix to toggle fog on and off.
(TODO) [REVERSED] LEVEL_UPDATE_FIX : Fixes an old bug where level is not immediately updated on screen.
(TODO) [FOG_TGA FIX]               : Fixes client crashing without a syserr while warping.

[YOSUN_PACK_FIX_001]        : "Python int too large to convert to C long" error fix.

+------------------------------------------------------------+
|                          * TODO *                          |
+------------------------------------------------------------+


+------------------------------------------------------------+
|                * Commands to set up MySQL *                |
+------------------------------------------------------------+
CREATE DATABASE account;
CREATE DATABASE log;
CREATE DATABASE common;
CREATE DATABASE player;
CREATE DATABASE hotbackup;
GRANT ALL PRIVILEGES ON *.* TO 'kaptan'@'localhost' WITH GRANT OPTION;

+------------------------------------------------------------+
|                  * Commands for GitHub *                   |
+------------------------------------------------------------+
Remove cached files in case gitignore doesn't ignore them:    git rm -r --cached *
Generate a Diff file of the latest commit:                    git show --pretty=format:%b > burayaisimyaz.diff