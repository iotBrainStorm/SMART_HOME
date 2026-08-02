# Graph Report - c:\Users\mrina\Documents\Arduino\ESP32\SMART_HOME.worktrees\copilot-worktree-2026-07-15T03-09-19  (2026-08-01)

## Corpus Check
- 43 files · ~82,672 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 221 nodes · 299 edges · 17 communities (16 shown, 1 thin omitted)
- Extraction: 78% EXTRACTED · 22% INFERRED · 0% AMBIGUOUS · INFERRED: 65 edges (avg confidence: 0.89)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- User & Device Configuration
- User Management & Switch Settings
- Restart & Reset Operations
- Sensor Automation & Icon Customization
- Firebase DB Rules & Admin Auth
- Admin Controls & Factory Reset
- Login & Authentication UI
- Automation Scheduling & Timers
- Firebase Setup & API Keys
- WiFi & IP Settings
- Network Status & Static IP
- WiFi Management Tab
- Sensor-based Automation
- Control Dashboard
- Location & Sun Times
- Firebase User Credentials
- Brand Identity

## God Nodes (most connected - your core abstractions)
1. `Administrator Tab` - 10 edges
2. `Settings - ESP Smart Home` - 9 edges
3. `ESP Smart Home Dashboard` - 9 edges
4. `Settings` - 9 edges
5. `SMART_HOME` - 8 edges
6. `Administrator APIs` - 8 edges
7. `Firebase Tab` - 7 edges
8. `Automation APIs` - 7 edges
9. `Switch Names Configuration Data` - 7 edges
10. `Automation Tab` - 6 edges

## Surprising Connections (you probably didn't know these)
- `WiFi Networking Tools` --semantically_similar_to--> `WiFi Tab`  [INFERRED] [semantically similar]
  README.md → data/config.html
- `Firebase Integration` --semantically_similar_to--> `Firebase Tab`  [INFERRED] [semantically similar]
  README.md → data/config.html
- `Settings Page` --semantically_similar_to--> `Settings - ESP Smart Home`  [INFERRED] [semantically similar]
  README.md → data/config.html
- `Automation Suite` --semantically_similar_to--> `Automation Tab`  [INFERRED] [semantically similar]
  README.md → data/config.html
- `Dashboard Overview` --semantically_similar_to--> `ESP Smart Home Dashboard`  [INFERRED] [semantically similar]
  README.md → data/index.html

## Hyperedges (group relationships)
- **Settings Tabs Navigation** — data_config_html_settings, data_config_html_switches_tab, data_config_html_automation_tab, data_config_html_wifi_tab, data_config_html_firebase_tab, data_config_html_user_tab, data_config_html_administrator_tab [EXTRACTED 1.00]
- **Automation Suite Panels** — data_config_html_add_timer, data_config_html_set_schedule, data_config_html_set_future_schedule, data_config_html_sensor_control [EXTRACTED 1.00]
- **Reset Safety Workflow** — data_config_html_reset_storage, data_config_html_reset_settings, data_config_html_factory_reset, data_config_html_admin_verification, data_config_html_boot_hold_requirement [EXTRACTED 1.00]
- **Automation Configuration Screens** — screenshots_addfutureschedules_future_schedule_screen, screenshots_addschedule_set_schedule_screen, screenshots_addsunrisesunsetautomations_sunrise_sunset_control_screen, screenshots_addtemperatureautomation_temperature_control_screen [INFERRED 0.95]
- **Credential Entry Screens** — screenshots_addespusers_add_esp_user_screen, screenshots_addfbusers_add_firebase_user_screen, screenshots_connectwifi_connect_wifi_screen [INFERRED 0.85]
- **Firebase Integration Configuration** — screenshots_firebaseurl_firebase_database_url, screenshots_firebasesecret_firebase_auth_token, screenshots_databaserules_firebase_database_rules, screenshots_firebaseusers_firebase_authentication_screen [INFERRED 0.85]
- **WiFi Network Management** — screenshots_dhcpip_ip_settings_screen, screenshots_dhcpip_dhcp_toggle, screenshots_forgetwifi_forget_wifi_screen, screenshots_forgetwifi_saved_wifi_credentials [INFERRED 0.85]
- **Admin Protected Operations** — screenshots_editadmin_admin_credentials, screenshots_databaserules_save_rules_action, screenshots_hardreset_factory_reset_action [INFERRED 0.75]
- **User Access Management** — screenshots_login_login_screen, screenshots_showespusers_registered_users_screen, screenshots_removeespusers_remove_user_screen [INFERRED 0.85]
- **Administrator Maintenance Controls** — screenshots_restartsetup_restart_device_screen, screenshots_resetui_reset_storage_screen, screenshots_settingsreset_reset_settings_screen, screenshots_schedulepriority_schedule_priority_screen [INFERRED 0.95]
- **Automation Scheduling Suite** — screenshots_showschedules_schedule_screen, screenshots_showfutureschedules_future_schedule_screen, screenshots_schedulepriority_schedule_priority_screen, screenshots_restartsetup_automatic_restart_schedule [INFERRED 0.85]
- **Sensor Automation Screen Group** — screenshots_showsensorautomations_sensor_automation_screen, screenshots_showsensorautomations_sensor_control_panel, screenshots_showsensorautomations_sensor_automation_configuration_data [EXTRACTED 1.00]
- **Firebase Users Screen Group** — screenshots_showfbusers_firebase_users_screen, screenshots_showfbusers_firebase_users_list, screenshots_showfbusers_saved_firebase_user_credentials [EXTRACTED 1.00]
- **IP Settings Screen Group** — screenshots_staticip_ip_settings_screen, screenshots_staticip_dhcp_toggle, screenshots_staticip_static_ip_configuration_form, screenshots_staticip_wifi_ip_configuration_data [EXTRACTED 1.00]
- **Reset Storage Screen Group** — screenshots_storagereset_reset_storage_screen, screenshots_storagereset_reset_storage_action, screenshots_storagereset_cleared_settings_scope [EXTRACTED 1.00]
- **Switch Names Screen Group** — screenshots_switchname_switch_names_screen, screenshots_switchname_switch_names_form, screenshots_switchname_switch_names_configuration_data [EXTRACTED 1.00]
- **Relay State Screen Group** — screenshots_switchstate_relay_state_screen, screenshots_switchstate_relay_state_form, screenshots_switchstate_relay_startup_state_configuration_data [EXTRACTED 1.00]
- **Switch Icon Dialog Group** — screenshots_switchiconchange_switch_icon_dialog, screenshots_switchiconchange_icon_selection_grid, screenshots_switchiconchange_switch_icons_configuration_data [EXTRACTED 1.00]
- **Time Setup Screen Group** — screenshots_timesetup_time_setup_screen, screenshots_timesetup_ntp_and_timezone_form, screenshots_timesetup_device_time_configuration_data [EXTRACTED 1.00]
- **Timer Screen Group** — screenshots_timer_timer_screen, screenshots_timer_timer_configuration_form, screenshots_timer_active_timers_list, screenshots_timer_active_timer_configuration_data [EXTRACTED 1.00]
- **WiFi Status Screen Group** — screenshots_wifinetworkstatus_network_status_screen, screenshots_wifinetworkstatus_network_status_panel, screenshots_wifinetworkstatus_wifi_connection_status_data [EXTRACTED 1.00]

## Communities (17 total, 1 thin omitted)

### Community 0 - "User & Device Configuration"
Cohesion: 0.06
Nodes (36): Settings, Settings Gear Icon, Add User Screen, Add User Form, Normal User Only Policy, Password Field, User ID Field, Add Firebase User Screen (+28 more)

### Community 1 - "User Management & Switch Settings"
Cohesion: 0.12
Nodes (26): Add User, Edit Admin, Registered Users, Relay Holding State, Remove User, Settings - ESP Smart Home, Switch Icons, Switch Names (+18 more)

### Community 2 - "Restart & Reset Operations"
Cohesion: 0.10
Nodes (26): Reset Storage Action, Reset Storage Checklist, Reset Storage Screen, Storage Reset Parameters, Automatic Restart Schedule, Manual Restart Action, Restart Device Screen, Restart Time Picker (+18 more)

### Community 3 - "Sensor Automation & Icon Customization"
Cohesion: 0.12
Nodes (23): Sensor Automation Configuration Data, Cleared Settings Scope, Reset Storage Action, Reset Storage Screen, Device Icon Set, Icon Selection Grid, Room Icon Set, Switch Icon Dialog (+15 more)

### Community 4 - "Firebase DB Rules & Admin Auth"
Cohesion: 0.12
Nodes (20): Database Rules Screen, Firebase Database Rules, Save Rules Action, Validate JSON Action, Admin Credentials, Edit Admin Screen, Save Admin Action, Authentication Token Screen (+12 more)

### Community 5 - "Admin Controls & Factory Reset"
Cohesion: 0.25
Nodes (16): Admin Verification, Administrator APIs, Administrator Tab, BOOT Hold Requirement, Device Name, Factory Reset, Location Setup, Reset Settings (+8 more)

### Community 6 - "Login & Authentication UI"
Cohesion: 0.19
Nodes (14): Login Screen, Password Field, Remember Me Option, Reset Admin Option, Sign In Action, User ID Field, Admin Protection Notice, Remove User Action (+6 more)

### Community 7 - "Automation Scheduling & Timers"
Cohesion: 0.30
Nodes (12): Add Timer, Automation APIs, Automation Tab, Humidity Control, Sensor Control, Set Future Schedule, Set Schedule, Sunset & Sunrise Control (+4 more)

### Community 8 - "Firebase Setup & API Keys"
Cohesion: 0.36
Nodes (9): Add Firebase User, Authentication Token, Database Rules, Database URL, Firebase APIs, Firebase Authentication, Firebase On/Off, Firebase Tab (+1 more)

### Community 9 - "WiFi & IP Settings"
Cohesion: 0.33
Nodes (7): DHCP Toggle, IP Settings Screen, Network Details, Save IP Settings Action, Forget WiFi Action, Forget WiFi Screen, Saved WiFi Credentials

### Community 10 - "Network Status & Static IP"
Cohesion: 0.43
Nodes (7): DHCP Toggle, IP Settings Screen, Static IP Configuration Form, WiFi IP Configuration Data, Network Status Panel, Network Status Screen, WiFi Connection Status Data

### Community 11 - "WiFi Management Tab"
Cohesion: 0.53
Nodes (6): Connect WiFi, Forget WiFi, IP Settings, Network Status, WiFi APIs, WiFi Tab

### Community 12 - "Sensor-based Automation"
Cohesion: 0.33
Nodes (6): Humidity Control Option, Sensor Automation Screen, Sensor Control Panel, Sunset and Sunrise Control Option, Switch 4 AC Automation Card, Temperature Control Option

### Community 13 - "Control Dashboard"
Cohesion: 0.50
Nodes (4): All Off Action, All On Action, Control Dashboard, Device Controls

### Community 14 - "Location & Sun Times"
Cohesion: 0.67
Nodes (4): Geographic Location, Location Setup Screen, Save Location Action, Sun Times

### Community 15 - "Firebase User Credentials"
Cohesion: 0.67
Nodes (3): Firebase Users List, Firebase Users Screen, Saved Firebase User Credentials

## Knowledge Gaps
- **64 isolated node(s):** `Confirmation Modal`, `Login API`, `ESP Wordmark Logo`, `ESP Brand Identity`, `Settings Gear Icon` (+59 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Settings - ESP Smart Home` connect `User Management & Switch Settings` to `Firebase Setup & API Keys`, `WiFi Management Tab`, `Admin Controls & Factory Reset`, `Automation Scheduling & Timers`?**
  _High betweenness centrality (0.060) - this node is a cross-community bridge._
- **Why does `Administrator Tab` connect `Admin Controls & Factory Reset` to `User Management & Switch Settings`?**
  _High betweenness centrality (0.024) - this node is a cross-community bridge._
- **Why does `Firebase Tab` connect `Firebase Setup & API Keys` to `User Management & Switch Settings`?**
  _High betweenness centrality (0.021) - this node is a cross-community bridge._
- **What connects `Confirmation Modal`, `Login API`, `ESP Wordmark Logo` to the rest of the system?**
  _64 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `User & Device Configuration` be split into smaller, more focused modules?**
  _Cohesion score 0.06190476190476191 - nodes in this community are weakly interconnected._
- **Should `User Management & Switch Settings` be split into smaller, more focused modules?**
  _Cohesion score 0.12 - nodes in this community are weakly interconnected._
- **Should `Restart & Reset Operations` be split into smaller, more focused modules?**
  _Cohesion score 0.10461538461538461 - nodes in this community are weakly interconnected._