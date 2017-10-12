#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2017 weihao <weihao@weihao-PC>
#
# Distributed under terms of the MIT license.

north_arr = []
south_arr = []
west_arr = []
east_arr = []

north_prepared = 0
south_prepared = 0
east_prepared = 0
west_prepared = 0

# 0 means no need.
# 1 means next should be north
# 2 means next should be south
# 3 means next should be west
# 4 means next should be east
left_go_next = 0

exempt = 0



fp = open('log', 'r')
while True:
    line = fp.readline()
    if exempt != 0:
        exempt = exempt - 1
    # reading completed
    if not line:
        if len(north_arr) != 0:     # check if all arrived cars leaved
            print('ERROR!!!')
        if len(south_arr) != 0:
            print('ERROR!!!')
        if len(west_arr) != 0:
            print('ERROR!!!')
        if len(east_arr) != 0:
            print('ERROR!!!')
        print('NO ERROR DETECTED.')
        break
    words = line.split(' ')
    if words[0] == 'car':
        if words[-1] == 'arrives.\n':
            if words[3] == 'WEST':
                west_arr.append(int(words[1]))      # when one car arrives, push it into its corresponding stack
            if words[3] == 'EAST':
                east_arr.append(int(words[1]))
            if words[3] == 'NORTH':
                north_arr.append(int(words[1]))
            if words[3] == 'SOUTH':
                south_arr.append(int(words[1]))
        if words[-1] == 'leaves.\n':
            if words[3] == 'WEST':
                if left_go_next != 3 and left_go_next != 0:   # if the left car didn't pass next as it supposed to
                    print('ERROR!')
                if north_prepared:      # record which direction is supposed to go next
                    left_go_next = 1
                else:
                    left_go_next = 0
                if south_prepared and not exempt:       # check if the rightmost car leaves first
                    print('ERROR!!')
                if int(words[1]) != west_arr[0]:    # check if first come first serve
                    print('ERROR!!!')
                else:
                    west_arr.pop(0)     # if fcfs, pop it out
                    west_prepared = 0       # no car prepared in the west
            if words[3] == 'EAST':
                if left_go_next != 4 and left_go_next != 0:
                    print('ERROR!')
                if south_prepared:
                    left_go_next = 2
                else:
                    left_go_next = 0
                if north_prepared and not exempt:
                    print("ERROR!!")
                if int(words[1]) != east_arr[0]:
                    print('ERROR!!!')
                else:
                    east_arr.pop(0)
                    east_prepared = 0
            if words[3] == 'NORTH':
                if left_go_next != 1 and left_go_next != 0:
                    print('ERROR!')
                if east_prepared:
                    left_go_next = 4
                else:
                    left_go_next = 0
                if west_prepared and not exempt:
                    print("ERROR!!")
                if int(words[1]) != north_arr[0]:
                    print('ERROR!!!')
                else:
                    north_arr.pop(0)
                    north_prepared = 0
            if words[3] == 'SOUTH':
                if left_go_next != 2 and left_go_next != 0:
                    print('ERROR!')
                if west_prepared:
                    left_go_next = 3
                else:
                    left_go_next = 0
                if east_prepared and not exempt:
                    print("ERROR!!")
                if int(words[1]) != south_arr[0]:
                    print('ERROR!!!')
                else:
                    south_arr.pop(0)
                    south_prepared = 0
        if words[-1] == 'prepared.\n':
            if words[3] == 'WEST':
                west_prepared = 1
            if words[3] == 'EAST':
                east_prepared = 1
            if words[3] == 'NORTH':
                north_prepared = 1
            if words[3] == 'SOUTH':
                south_prepared = 1
    elif words[0] == 'DEADLOCK:':
        exempt = 4
