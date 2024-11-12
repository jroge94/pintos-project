# -*- perl -*-
use strict;
use warnings;
use tests::tests;

our %args;

# Test name and description
$args{'name'} = 'tell-normal';
$args{'description'} = 'Tests the \'tell\' system call';

# Program to run
$args{'test_program'} = 'tell-normal';

# Expected output
$args{'stdout'} = <<'END_OUTPUT';
Wrote 'Hello, PintOS!' to testfile.txt
Current position after write: 14
Seeking to position 0
Current position: 0
Seeking to position 5
Current position: 5
Seeking to position 7
Current position: 7
tell-normal: pass
END_OUTPUT
pass;