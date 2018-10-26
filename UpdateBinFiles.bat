REM - Script to prepare for Release

SET BINTARGET=bin
SET REGFREECOM=\ARP\BridgeLink\RegFreeCOM


REM - COM DLLs
xcopy /Y /d Convert\Convert.dll				%BINTARGET%\AutomationDLLs\Win32\
xcopy /Y /d Convert\Convert.dll				%BINTARGET%\AutomationDLLs\x64\
xcopy /Y /d %REGFREECOM%\Win32\Release\PGSuper*.dll		%BINTARGET%\AutomationDLLs\Win32\
xcopy /Y /d %REGFREECOM%\x64\Release\PGSuper*.dll		%BINTARGET%\AutomationDLLs\x64\

REM - Extension Agents
REM - WSDOT
xcopy /Y /d %REGFREECOM%\Win32\Release\WSDOTAgent.dll	%BINTARGET%\Extensions\WSDOT\Win32\
xcopy /Y /d %REGFREECOM%\x64\Release\WSDOTAgent.dll	%BINTARGET%\Extensions\WSDOT\x64\

REM - TXDOT
xcopy /Y /d %REGFREECOM%\Win32\Release\TxDOTAgent.dll	%BINTARGET%\Extensions\TxDOT\Win32\
xcopy /Y /d %REGFREECOM%\x64\Release\TxDOTAgent.dll	%BINTARGET%\Extensions\TxDOT\x64\
xcopy /Y /d TOGA.chm				%BINTARGET%\Extensions\TxDOT\
xcopy /Y /d TxDOTAgent\TogaTemplates\*.pgs		%BINTARGET%\Extensions\TxDOT\TogaTemplates\
xcopy /Y /d TxDOTAgent\TogaTemplates\*.togt		%BINTARGET%\Extensions\TxDOT\TogaTemplates\
xcopy /Y /d TxDOTAgent\TogaTemplates\*.ico		%BINTARGET%\Extensions\TxDOT\TogaTemplates\

REM - KDOT
xcopy /Y /d %REGFREECOM%\Win32\Release\KDOTExport.dll   %BINTARGET%\Extensions\KDOT\Win32\
xcopy /Y /d %REGFREECOM%\x64\Release\KDOTExport.dll	    %BINTARGET%\Extensions\KDOT\x64\

REM - Image files
xcopy /Y /d images\*.gif				%BINTARGET%\images\
xcopy /Y /d images\*.jpg				%BINTARGET%\images\
xcopy /Y /d images\*.png				%BINTARGET%\images\

REM - Application files
xcopy /Y /d md5deep.exe			  	%BINTARGET%\App\
xcopy /Y /d %REGFREECOM%\Win32\Release\MakePgz.exe 	%BINTARGET%\App\Win32\
xcopy /Y /d %REGFREECOM%\x64\Release\MakePgz.exe  	%BINTARGET%\App\x64\
xcopy /Y /d PGSuper.tip				%BINTARGET%\App\
xcopy /Y /d License.txt				%BINTARGET%\App\
xcopy /Y /d \ARP\BridgeLink\PGSuper.chm		%BINTARGET%\App\
xcopy /Y /d Trucks.pgs				%BINTARGET%\App\

REM - Configuration Files
xcopy /Y /d Configurations\WSDOT.lbr				%BINTARGET%\Configurations\
xcopy /Y /d Configurations\PGSuper\WSDOT\WF_DG-Girders\*.ico	%BINTARGET%\Configurations\PGSuper\WF_DG-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\WF_DG-Girders\*.pgt	%BINTARGET%\Configurations\PGSuper\WF_DG-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\WF_TDG-Girders\*.ico	%BINTARGET%\Configurations\PGSuper\WF_TDG-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\WF_TDG-Girders\*.pgt	%BINTARGET%\Configurations\PGSuper\WF_TDG-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\W-Girders\*.ico		%BINTARGET%\Configurations\PGSuper\W-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\W-Girders\*.pgt		%BINTARGET%\Configurations\PGSuper\W-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\U-Girders\*.ico		%BINTARGET%\Configurations\PGSuper\U-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\U-Girders\*.pgt		%BINTARGET%\Configurations\PGSuper\U-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\WF-Girders\*.ico	%BINTARGET%\Configurations\PGSuper\WF-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\WF-Girders\*.pgt	%BINTARGET%\Configurations\PGSuper\WF-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\WBT-Girders\*.ico	%BINTARGET%\Configurations\PGSuper\WBT-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\WBT-Girders\*.pgt	%BINTARGET%\Configurations\PGSuper\WBT-Girders\
xcopy /Y /d Configurations\PGSuper\WSDOT\Deck_Bulb_Tees\*.ico	%BINTARGET%\Configurations\PGSuper\Deck_Bulb_Tees\
xcopy /Y /d Configurations\PGSuper\WSDOT\Deck_Bulb_Tees\*.pgt	%BINTARGET%\Configurations\PGSuper\Deck_Bulb_Tees\
xcopy /Y /d Configurations\PGSuper\WSDOT\MultiWeb\*.ico		%BINTARGET%\Configurations\PGSuper\MultiWeb\
xcopy /Y /d Configurations\PGSuper\WSDOT\MultiWeb\*.pgt		%BINTARGET%\Configurations\PGSuper\MultiWeb\
xcopy /Y /d Configurations\PGSuper\WSDOT\Slabs\*.ico		%BINTARGET%\Configurations\PGSuper\Slabs\
xcopy /Y /d Configurations\PGSuper\WSDOT\Slabs\*.pgt		%BINTARGET%\Configurations\PGSuper\Slabs\

xcopy /Y /d Configurations\PGSplice\WSDOT\I-Beams\*.ico          %BINTARGET%\Configurations\PGSplice\I-Beams\
xcopy /Y /d Configurations\PGSplice\WSDOT\I-Beams\*.spt          %BINTARGET%\Configurations\PGSplice\I-Beams\
xcopy /Y /d Configurations\PGSplice\WSDOT\U-Beams\*.ico          %BINTARGET%\Configurations\PGSplice\U-Beams\
xcopy /Y /d Configurations\PGSplice\WSDOT\U-Beams\*.spt          %BINTARGET%\Configurations\PGSplice\U-Beams\
