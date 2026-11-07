General {#ug_dialogs_load_rating_options_general}
==============================================

Define the general load rating parameters

Item | Description
------|----------------
Load Rating Criteria | Use the drop down list to select a load rating criteria.
Load Rating Event | Use the drop down list to select the event when load rating vehicles are applied to the bridge.
System Factor | Input system factors for flexure and shear. See MBE 6A.4.2.4 for guidance.
Average Daily Truck Traffic | Enter the average daily truck traffic (ADTT) for one direction. If the ADTT is unknown, input the keyword "Unknown".
Apply pedestrian live load concurrently with vehicular live loads | Check this box if pedestrian live load is to be included in the load rating analysis. See MBE 6A.2.3.4 for guidance.
Rating Type | Select the type of load rating to be performed by check the appropriate boxes.

MBE Fourth Edition project allowances
------------------------------------
These settings are saved with the project and restored by undo and redo. They default to enabled in new projects. Existing files without the settings load with the allowances disabled; select them explicitly after review. Earlier MBE editions do not use these allowances.

* **Use zero strain in uncracked sections (6A.5.8)**: for MCFT equations, set longitudinal strain to zero only when the section is uncracked at the evaluated load, producing theta = 29 degrees. Recheck cracking at each trial truck weight.
* **Neglect size effect for qualifying prestressed sections (6A.5.8)**: for MCFT equations, omit the size-effect reduction only when fpc/fc prime is at least 0.02 and prestressing is present. This is independent of the zero-strain setting.
* **Recalculate K using reduced rating demand (C6A.5.6)**: recalculate K and RF consistently for eligible minimum-reinforcement-controlled ratings below 1.0. Permanent loads remain fixed. Turning it off retains K from the original loading.

The load-rating details report lists the selected allowances. The MCFT choices do not change the table-based or Vci/Vcw methods. These settings do not enable segmental bridge evaluation or refined transverse permit analysis.
