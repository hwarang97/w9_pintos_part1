# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
hello syscall
write-syscall: exit(0)
EOF
pass;
