#! /usr/bin/python3

import sys
import getopt

def usage():
    print('make-github-grade.py -i <grade> [-o <grade.result>]')

def make_grade(argv):

    try:
        opts, args = getopt.getopt(argv[1:], "i:o:")
    except getopt.GetoptError:
        usage()       # print help information and exit
        return -1

    input_filename = 'grade'
    grade_result_name = None
    for o, a in opts:
        if o == "-i":
            input_filename = a
        elif o == "-o":
            grade_result_name = a
        else:
            print("Unknown command line option '%s'" % o)
            usage()
            return -1

    # we're looking for something like:
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    # 
    # SUMMARY BY TEST SET
    # 
    # Test Set                                      Pts Max  % Ttl  % Max
    # --------------------------------------------- --- --- ------ ------
    # tests/userprog/Rubric.functionality            59/124  16.7%/ 35.0%
    # tests/userprog/Rubric.robustness               70/ 91  19.2%/ 25.0%
    # tests/userprog/no-vm/Rubric                     0/  1   0.0%/ 10.0%
    # tests/filesys/base/Rubric                       0/ 30   0.0%/ 30.0%
    # --------------------------------------------- --- --- ------ ------
    # Total                                                  35.9%/100.0%
    # 
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    #
    # SUMMARY OF INDIVIDUAL TESTS
    # 
    # Functionality of system calls (tests/userprog/Rubric.functionality):
    # 	- Test argument passing on Pintos command line.
    # 	     3/ 3 tests/userprog/args-none
    # 	     3/ 3 tests/userprog/args-single

    delim = '--------------------------------------------- --- --- ------ ------'

    collect = False
    data = []
    f = open(input_filename, 'r')
    for l in f:
        if l.find(delim) != -1:
            if collect:
                break
            collect = True
            continue
        if collect:
            parts = l.split()
            if len(parts) == 5:
                parts[1] = '%s%s' % (parts[1], parts[2])
                parts.remove(parts[2])
            data.append(parts)
    f.close()

    collect = 0
    maxlen = 0
    data2 = []
    f = open(input_filename, 'r')
    for l in f:
        if l.find(delim) != -1:
            collect = collect + 1
            continue
        if collect >= 2:
            for d in data:
                if l.find(d[0]) != -1:
                    name = l.split('(')[0][:-1]
                    maxlen = max(maxlen, len(name))
                    data2.append([name, d[1], d[2], d[3]])
    f.close()

    def print_delim(start, middle, end, line, columns):
        print(start, end='')
        for c, width in enumerate(columns):
            for i in range(width):
                print(line, end='')
            if c == len(columns)-1:
                print(end, end='')
            else:
                print(middle, end='')
        print()

    def print_line(texts, columns):
        print('│', end='')
        for i in range(len(texts)):
            format = ' %%%ds ' % (columns[i] - 2)
            print(format % texts[i], end='')
            print('│', end='')
        print()

    COLORS = {
        "reset": "\x1b[0m",
        "cyan": "\x1b[36m",
        "green": "\x1b[32m",
        "red": "\x1b[31m",
        "yellow": "\x1b[33m",
        "magenta": "\x1b[35m",
    }

    all_percent = 0
    overall_percent = 0
    overall_tests = 0
    overall_passed = 0
    overall_extra_tests = 0
    overall_extra_passed = 0

    columns = [maxlen+2, 9, 18, 11]

    print()
    print('%sTest Summary%s' % (COLORS['magenta'], COLORS['reset']))

    print_delim('┌', '┬', '┐', '─', columns)
    print_line(['Test Category', 'Points', 'Test Score', 'Max Score'], columns)
    print_delim('╞', '╪', '╡', '═', columns)
    for i, d in enumerate(data2):
        score = float(d[2].split('%')[0])
        if d[0].find('extra') != -1 and score == 0:
            continue
        overall_percent += score
        counts = d[1].split('/')
        if d[0].find('extra') != -1:
            overall_extra_tests += int(counts[1])
            overall_extra_passed += int(counts[0])
        else:
            overall_tests += int(counts[1])
            overall_passed += int(counts[0])
        maxscore = float(d[3].split('%')[0])
        all_percent += maxscore
        print_line([d[0], d[1], '%.1f (%.1f%%)' % (score, score / maxscore * 100), '%.1f' % maxscore], columns)
        if i == len(data2) - 1:
            print_delim('╞', '╪', '╡', '═', columns)
        else:
            print_delim('├', '┼', '┤', '─', columns)
    
    print_line(['Total Grade', 
                '%d/%d' % (overall_passed + overall_extra_passed, overall_tests + overall_extra_tests), 
                '%.1f%%' % overall_percent, '%.1f' % all_percent], columns)
    print_delim('└', '┴', '┘', '─', columns)

    if grade_result_name:
        f = open(grade_result_name, 'w+')
        if overall_passed == overall_tests:
            print('0', file=f)
        else:
            print('1', file=f)
        f.close()
    return 0

###############################################################################
def main(argv = None):
    if argv is None:
        argv = sys.argv
    return make_grade(argv)

# main entry point is here
if __name__ == "__main__":
    sys.exit(main())

# {
#   runner: "Test Runner 1",
#   results: {
#     version: 1,
#     status: "pass",
#     tests: [
#       {
#         name: "Test 1",
#         status: "pass",
#         message: null,
#         line_no: null,
#         execution_time: "0ms",
#         score: 1,
#       },
#       {
#         name: "Test 2",
#         status: "fail",
#         message: "Expected output not matched",
#         line_no: 10,
#         execution_time: "5ms",
#         score: 0,
#       },
#     ],
#     max_score: 2,
#   },
# }
# 