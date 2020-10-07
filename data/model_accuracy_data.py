import numpy as np

class Data:
    def __init__(self,name,x,y,mins = None,maxs = None):
        self.label = name
        self.x = x
        self.y = y

        ## calculate mins & maxs
        self.mins = [0] * len(x)
        self.maxs = [0] * len(x)

        if mins or maxs:
            for i in range(len(x)):
                if mins:
                    self.mins[i] = abs(mins[i] - y[i])
                if maxs:
                    self.maxs[i] = abs(maxs[i] - y[i])
        return

    def get_err(self):
        raw_data = [self.mins,self.maxs]
        return np.array(raw_data)

ycsbh_models  = [1,20,100,200,500,1000,2000,5000,10000,20000]
ycsbh_avg_err = [5491.51,958.4990,463.5075,338.3308,226.4714,173.9543,134.5346,77.1288,54.3931,38.69]
ycsbh_min_err = [2012,391,245,117,21,9,22,7,6,6]
ycsbh_max_err = [5492,1283,807,607,414,304,246,162,108,84]

all_data = { "ycsbh" : Data("ycsbh",ycsbh_models,ycsbh_avg_err,ycsbh_min_err,ycsbh_max_err) }
