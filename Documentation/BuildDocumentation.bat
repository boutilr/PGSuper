REM Use subroutine to do hard work of generating html with search capabilities

REM - Build PGSuper Documentation
set PROJECT=PGSuper
call :subGenHtml

REM - Build PGSplice Documentation
set PROJECT=PGSplice
call :subGenHtml

REM - Build PGSLibrary Documentation
set PROJECT=PGSLibrary
call :subGenHtml

REM - Build the Documentation Map File
cd %ARPDIR%\PGSuper\Documentation
%ARPDIR%\BridgeLink\RegFreeCOM\x64\Release\MakeDocMap PGSuper

REM - Make a copy of the DM file for PGSplice and PGS Library
copy PGSuper.dm PGSplice.dm
copy PGSuper.dm PGSLibrary.dm

REM - Copy the documentation to BridgeLink
rmdir /S /Q %ARPDIR%\BridgeLink\Docs\PGSuper\%1\
mkdir %ARPDIR%\BridgeLink\Docs\PGSuper\%1\
xcopy /s /y /d %ARPDIR%\PGSuper\Documentation\PGSuper\doc\html\* %ARPDIR%\BridgeLink\Docs\PGSuper\%1\
copy %ARPDIR%\PGSuper\Documentation\PGSuper.dm %ARPDIR%\BridgeLink\Docs\PGSuper\%1\

rmdir /S /Q %ARPDIR%\BridgeLink\Docs\PGSplice\%1\
mkdir %ARPDIR%\BridgeLink\Docs\PGSplice\%1\
xcopy /s /y /d %ARPDIR%\PGSuper\Documentation\PGSplice\doc\html\* %ARPDIR%\BridgeLink\Docs\PGSplice\%1\
copy %ARPDIR%\PGSuper\Documentation\PGSplice.dm %ARPDIR%\BridgeLink\Docs\PGSplice\%1\

rmdir /S /Q %ARPDIR%\BridgeLink\Docs\PGSLibrary\%1\
mkdir %ARPDIR%\BridgeLink\Docs\PGSLibrary\%1\
xcopy /s /y /d %ARPDIR%\PGSuper\Documentation\PGSLibrary\doc\html\* %ARPDIR%\BridgeLink\Docs\PGSLibrary\%1\
copy %ARPDIR%\PGSuper\Documentation\PGSLibrary.dm %ARPDIR%\BridgeLink\Docs\PGSLibrary\%1\

goto :eof

REM subroutine to do hard work of generating search-enabled html
:subGenHtml
cd %ARPDIR%\PGSuper\Documentation\%PROJECT%
rmdir /S /Q doc
doxygen Doxygen.dox

rem ** The code below replaces Doxygen's search.js file with version from https://github.com/divideconcept/DoxygenDeepSearch
rem    into our html help system to enable plain text search on both browser and server.
cd ..
copy /Y %ARPDIR%\WBFL\DoxygenDeepSearch\search.js %PROJECT%\doc\html\search\search.js

rem ** Use of the deep search search.js requires modification of the search.html generated by Doxygen
rem    search xml data is appended as a script to the end of the file
set SEARCH_HTML=%PROJECT%\doc\html\search.html  
echo ^<script id="searchdata" type="text/xmldata"^> >> %SEARCH_HTML%
type %PROJECT%\doc\searchdata.xml >> %SEARCH_HTML%
echo ^</script^> >> %SEARCH_HTML%
exit /B 0

:eof