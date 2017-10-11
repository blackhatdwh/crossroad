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

fp = open('log', 'r')
while True:
    line = fp.readline()
    # reading completed
    if not line:
        if len(north_arr) != 0:
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
                west_arr.append(int(words[1]))
            if words[3] == 'EAST':
                east_arr.append(int(words[1]))
            if words[3] == 'NORTH':
                north_arr.append(int(words[1]))
            if words[3] == 'SOUTH':
                south_arr.append(int(words[1]))
        if words[-1] == 'leaves.\n':
            if words[3] == 'WEST':
                if int(words[1]) != west_arr[0]:
                    print('ERROR!!!')
                else:
                    west_arr.pop(0)
            if words[3] == 'EAST':
                if int(words[1]) != east_arr[0]:
                    print('ERROR!!!')
                else:
                    east_arr.pop(0)
            if words[3] == 'NORTH':
                if int(words[1]) != north_arr[0]:
                    print('ERROR!!!')
                else:
                    north_arr.pop(0)
            if words[3] == 'SOUTH':
                if int(words[1]) != south_arr[0]:
                    print('ERROR!!!')
                else:
                    south_arr.pop(0)
    elif words[0] == 'DEADLOCK:':
        pass
