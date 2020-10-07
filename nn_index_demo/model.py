#!/usr/bin/env python

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim

import progressbar

from loader import *

class Net(nn.Module):

    def __init__(self):
        super(Net, self).__init__()
        # an affine operation: y = Wx + b
        self.fc1 = nn.Linear(1, 16).double()
        self.fc2 = nn.Linear(16, 1).double()
        #self.fc2 = nn.Linear(120, 84).double()
        #self.fc3 = nn.Linear(84, 1).double()

    def forward(self, x):
        #x = self.fc1(x)
        x = F.relu(self.fc1(x))
        x = self.fc2(x)
        #print("forward ",x," using w: " ,self.get_parameters())
        #x = F.relu(self.fc2(x))
        #x = self.fc3(x)
        return x

    def get_parameters(self):
        res = []
        for p in self.parameters():
            if p.requires_grad:
                res.append(p.data)
        return res


    def num_flat_features(self, x):
        size = x.size()[1:]  # all dimensions except the batch dimension
        num_features = 1
        for s in size:
            num_features *= s
        return num_features


net = Net()
print(net)

## prepare the dataset
num_keys = 10000
input = input_of_ycsb(num_keys)
output = get_sorted_label(input)

input,labels = raw_input_to_minibatches(input,output,1000)

## copy a copy of input
temp_input = input

criterion = nn.MSELoss()
#criterion = nn.L1Loss()
#optimizer = optim.SGD(net.parameters(), lr=0.0005, momentum=0.0)
optimizer = optim.Adam(net.parameters(),lr = 0.0001)

epoches = 10000
a = progressbar.ProgressBar(maxval = epoches)
a.start()

for epoch in range(epoches):  # loop over the dataset multiple times
    running_loss = 0.0
    for i, data in enumerate(input):
        inputs = torch.DoubleTensor(data)
        inputs = inputs.view(-1,1)
        label = torch.DoubleTensor(labels[i])
        label = label.view(-1,1)

        outputs = net(inputs)

        loss = criterion(outputs, label)
        loss.backward()
        optimizer.step()

        running_loss += loss.item()
        if i % 2000 == 1999:    # print every 2000 mini-batches
            print('[%d, %5d] loss: %.3f' %
                  (epoch + 1, i + 1, running_loss / 2000))
            running_loss = 0.0
    a.update(epoch)
a.finish()

try:
    for d in temp_input:
        print('predict for : ',d,"; value: ",net(torch.DoubleTensor((d,))).data.tolist())
except:
    for d in temp_input:
        ## treat it as a scalar
        for i,j in enumerate(d):
            if i < 10:
                print('predict for : ',j,"; value: ",net(torch.DoubleTensor((j,))).data.tolist())
