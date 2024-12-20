Command Line Options {#ug_command_line_options}
==============================================
Command line options are parameters that are passed to the program when execution begins. The options alter the normal behavior of the software or provide some other special functionality. This chapter describes the command line options.

PGSuper Command Options
------------------------

PGSuper supports the following command options

Option | Description
-------|--------------
/Configuration=<i>ServerName</i>:<i>PublisherName</i> | Sets the application configuration.
/TestR <i>filename.pgs</i>| Generates NCHRP 12-50 test results for all problem domains
/Test<i>n</i> <i>filename.pgs</i> | Generates NCHRP 12-50 test results for a specified problem domain. Substitute the problem domain ID for <i>n</i>.

> NOTE: The /Configuration option is not unique to PGSuper. Other BridgeLink applications also have a /Configuration option. Use the /App=PGSuper option along with /Configuration to configure PGSuper.

               BridgeLink.exe /App=PGSuper /Configuration="WSDOT":"WSDOT"

Running in Silent Modes (Experimental)
--------------------------------------
With these options PGSuper can be run from the command line without showing User Interface (UI) windows. This is to facilitate running the program from scripts.

Option | Description
-------|--------------
/NoUI   | Normal windows are not displayed during execution. PGsuper's progress messages are redirected (displayed) on the command window (aka, terminal) where the program was started.
/NoUIS   | (Silent Mode) Normal windows are not displayed during execution, and no messages are posted anywhere during execution.

#### Notes ####
  - The program must run to normal completion in order for the Windows process to terminate. If the program does not terminate, the process will remain (orphaned) until the task is ended from the Windows Task Manager or Windows is shut down. Hence, it only makes sense to use these options for PGSuper's testing commands with error-free input files that will run to completion.
  - If an error occurs during execution, the program may show an error dialog. This dialog must be dismissed manually in order for the program to continue.

> TIP: You should always try running your command line without the above silent options to make sure the program will run to termination.

TxDOT Data Exporter
-------------------

> NOTE: You must select the TxDOT components at installation time in order to use this feature.

When the TxDOT data export options are given on the command line, PGSuper will create an output file that contains the TxDOT CAD data. The output generated when using the "Tx" options is very similar to that which is generated by PSTRS14. This feature can be used for batch processing.

### Command Line Options ("Tx" Options) ###
To run PGSuper with the Tx options, use a command with the following format:

               BridgeLink.exe /Option input_file.pgs Output_file  \<span\> \<girder\>

Where:
* /PGSuper is the BridgeLink command option that directs the remainder of the command line to PGSuper for processing
* Option (case in-sensitive) can be:
    - TxA, Run analysis of current data, output standard CAD format
    - TxAx, Run analysis of current data, output extended testing format
    - TxD, Run flexure design, output standard CAD format
    - TxDx, Run flexure design, output extended testing format
    - TxDS, Run flexure and shear design, output extended testing format
    - TxAo, Same as TxA but will overwrite output file instead of appending
    - TxAxo, Same as TxAx but will overwrite output file instead of appending
    - TxDo, Same as TxD but will overwrite output file instead of appending
    - TxDxo, Same as TxDx but will overwrite output file instead of appending
* Input_file.pgs is the input file to be processed
* Output_file is the name of the output file. Existing files will be overwritten.
* \<span\> is a single integer (e.g., 1, 2, ...) defining the span where the girder is, or "All" to run all spans. 
* \<girder\> is a single character (e.g., a, B, f, ...) defining the girder number, or "All" to run all girders in a span (or all girders in all spans if span option is also "All").

#### Notes ####
If an error occurs while running PGSuper, a .err file (e.g., "Route66.err") attempting to explain the problem will be created. Partial data will likely be in the output file.

Library Entry conflicts will cause processing to cease. You need to make sure that there are no library conflicts in the input file before running them from the command line.

> NOTE The file format for this command has been defined by TxDOT and can be changed by them without notice.

### Examples ###
An example of a command line to run a design for span 1, girder C using input file "Route66.pgs" and creating an extended output file "Route66.txt" is as follows:

                 BridgeLink.exe /TxDx Route66.pgs Route66.txt 1 C

Here is another example to run an analysis using standard output format for all beams in Span 2:

                  BridgeLink.exe /TxA Route66.pgs Route66.txt 2 ALL

### Output File Format ###
Output files are generated in "standard" or "enhanced" format. These are space-delimited text files (fields are separated by a single blank space). The data formats for draped and debonded strand layouts are different by nature. Refer to the TxDOT IGND and UBND girder design standard plan sheets for more information about data and formatting.

> TIP: The extended version of the file contains extra data and a table header that describes the content.

