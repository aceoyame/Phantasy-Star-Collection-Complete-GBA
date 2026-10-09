"""Original controller data that was incorrectly emitted as a sprite mapping.

These addresses are palette words, position/motion data, routine offsets or
long frame-pointer lists. The original controller consumes them; the data table
itself is never a hardware sprite. The selected concrete child mappings remain
in the neighboring sequence records. Their complete scheduling is a separate
controller audit. loc_1308C is deliberately retained: its word declarations
encode a real mapping, loaded into $C(a4).
"""
METADATA={
 0x15A2C:'palette cycle',0x15D62:'palette cycle',
 0x3794C:'routine dispatch',0x17C8E:'frame pointer list',
 0x16AFC:'motion offsets',0x1E6CC:'party X coordinates',0x1DFBE:'motion offsets',
 0x1CC04:'motion offsets',0x1CBA4:'motion offsets',0x23896:'controller durations',
 0x221BA:'frame pointer list',0x21FD2:'frame pointer list',0x2176E:'party X coordinates',
 0x2CC50:'frame pointer list',0x2CB08:'party X coordinates',0x2C84A:'party X coordinates',0x208DE:'party X coordinates',
 0x2B41C:'routine dispatch',0x28BD2:'frame pointer list',0x278A8:'routine dispatch',
 0x3236C:'routine dispatch',0x3168E:'routine dispatch',0x317DC:'palette words',0x31836:'palette words',
 0x303C6:'routine dispatch',0x306F4:'palette words',0x314A6:'frame pointer list',0x2F5F0:'routine dispatch',
 0x27944:'routine dispatch',0x27D22:'motion offsets',0x2EFD4:'object spawn records',0x312CA:'routine dispatch',
 0x2FA1E:'object coordinates',0x2EDB8:'palette words',0x2ED98:'palette words',
 0x2E022:'frame pointer list',0x2E12C:'controller timing and motion',0x2E41E:'animation table pointer list',0x35424:'motion offsets',
}
