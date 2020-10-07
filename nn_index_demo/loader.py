#!/usr/bin/env python
"""
 This module has 2 part1:
 - 1) Various loaders to generate data-set
 - 2) Uitlities to convert raw keys into minibatches
"""
import unittest

def input_from_txt(file_name, num):
    f = open(file_name)

    all_data = []
    for i,l in enumerate(f):
        if i >= num:
            break
        all_data.append(float(l))
    f.close()
    all_data.sort()
    return all_data

def input_of_ycsb(num):
    keys = []
    for i in range(num):
        keys.append(i)
    return keys

def get_sorted_label(input):
    labels = []
    for i in range(len(input)):
        labels.append(i)
    return labels

def raw_input_to_minibatches(input,output,batch_num):
    assert (len(input) == len(output))

    a_inputs = []
    a_outpus = []

    batch_num = min(batch_num,len(input))
    for i in range(0,len(input),batch_num):
        if i + batch_num <= len(input):
            a_inputs.append(input[i:i + batch_num])
            a_outpus.append(output[i:i + batch_num])

    if len(input) % batch_num != 0:
        idx = int(len(input) / batch_num) * batch_num
        num = len(input) % batch_num
        a_inputs.append(input[idx:idx + num])
        a_outpus.append(output[idx:idx + num])
    return a_inputs,a_outpus

def sanity_check_mini_batch(input,mini_batchs):
    sum = 0
    for i in mini_batchs:
        sum += len(i)
    return sum == len(input)

class TestStringMethods(unittest.TestCase):

    def test_mini_batch(self):
        keys = [1,2,3,4]
        labels = [1,2,3,4]

        batches = [1,2,3,4]

        for b in batches:
            k,l = raw_input_to_minibatches(keys,labels,b)
            self.assertTrue(sanity_check_mini_batch(keys,k))

if __name__ == '__main__':
    unittest.main()
