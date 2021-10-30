import easygui as eg
import configparser
import os
import types
from itertools import chain
import sys
#import ValueError

def process(fings, n=7, acc=0):
    if(n==-1):
        return [acc]
    c=fings[0]
    if c=='o':
        return process(fings[1:],n-1, acc)
    if c=='x':
        return process(fings[1:],n-1, acc+2**n)
    if c=='*':
        return [process(fings[1:],n-1, acc+2**n), process(fings[1:],n-1, acc)]
    else:
        print(c)
        raise ValueError('Only x or o or * are allowed in fingers.')

def flatten(ls):
    try:
        print (ls)
        out = list(chain(*ls))
        print (out)
    except TypeError:
        print ("Error")
        return ls
    return out


from copy import deepcopy

def flatten_list(nested_list):
    """Flatten an arbitrarily nested list, without recursion (to avoid
    stack overflows). Returns a new list, the original list is unchanged.
    >> list(flatten_list([1, 2, 3, [4], [], [[[[[[[[[5]]]]]]]]]]))
    [1, 2, 3, 4, 5]
    >> list(flatten_list([[1, 2], 3]))
    [1, 2, 3]
    """
    nested_list = deepcopy(nested_list)

    while nested_list:
        sublist = nested_list.pop(0)

        if isinstance(sublist, list):
            nested_list = sublist + nested_list
        else:
            yield sublist

def ConfigSectionMap(section):
    dict1 = {}
    options = Config.options(section)
    for option in options:
        try:
            dict1[option] = Config.get(section, option)
            if dict1[option] == -1:
                DebugPrint("skip: %s" % option)
        except:
            print("exception on %s!" % option)
            dict1[option] = None
    return dict1

infile = eg.fileopenbox(msg='Please locate the .ini file',
                        title='Specify File', default='./notes.ini')

Config = configparser.ConfigParser()
Config.read(infile)
print(Config.sections())

#print(ConfigSectionMap('MIDI'))
#print(ConfigSectionMap('FINGERING'))

os.chdir('../')
f = open('src/notes.h','w')

all_fingers=[0 for i in range(256)]

for key in  ConfigSectionMap('PICADOS'):
    #print(key)
    #print(ConfigSectionMap('FINGERING')[key])
    midi_note=int(ConfigSectionMap('MIDI')[key])
    fingers=ConfigSectionMap('PICADOS')[key].split()
    print (midi_note)
    print (fingers)
    print("---")
    for fs in fingers:
        indexes=flatten_list(process(fs))
        for i in indexes:
            all_fingers[i]=midi_note

for key in  ConfigSectionMap('MIDI'):
    #print(key)
    #print(ConfigSectionMap('FINGERING')[key])
    midi_note=int(ConfigSectionMap('MIDI')[key])
    fingers=ConfigSectionMap('FINGERING')[key].split()
    print (midi_note)
    print (fingers)
    print("---")
    for fs in fingers:
        indexes=flatten_list(process(fs))
        for i in indexes:
            all_fingers[i]=midi_note


f.write("constexpr uint8_t note_table[256] {")

for element in all_fingers:
    f.write("{},".format(element))

f.write("};\n")
f.close()

cmd = "platformio run -t upload"
#cmd = "platformio run"
import subprocess
process = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
output, error = process.communicate()

if error != None:
    raise ValueError('Error Generating firmware.');


print(output)
print(error)


f = open('src/notes.h','r')
print(f.read())
f.close()

