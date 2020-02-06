MessageIdTypedef=DWORD

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
    )


FacilityNames=(System=0x0:FACILITY_SYSTEM
    Runtime=0x2:FACILITY_RUNTIME
    Stubs=0x3:FACILITY_STUBS
    Io=0x4:FACILITY_IO_ERROR_CODE
)

LanguageNames=(
English=0x409:MSG00409
)

MessageId=1000
Severity=Error
Facility=Runtime
SymbolicName=MSG_EXCEPTION
Language=English
%2
.

MessageId=1001
Severity=Informational
Facility=Runtime
SymbolicName=MSG_STARTING
Language=English
%2
.

MessageId=1002
Severity=Informational
Facility=Runtime
SymbolicName=MSG_RUNNING
Language=English
%2
.

MessageId=1003
Severity=Informational
Facility=Runtime
SymbolicName=MSG_STOPPING
Language=English
%2
.

MessageId=1004
Severity=Informational
Facility=Runtime
SymbolicName=MSG_STOPPED
Language=English
%2
.
