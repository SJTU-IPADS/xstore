"""
given data int this format:
{
 k : (thpt,lat)
 k1 : (thpt1,lat1),
 ...
}

return [thpt,thpt1,...], [lat,lat1,...]
"""
def flat_latency_thpt(data):
    thpts = []
    lats = []
    for k in data:
        v = data[k]
        thpt,latency = v
        thpts.append(thpt)
        lats.append(latency)
    return thpts,lats


"""
Given data [(a0,a1,...),(b0,b1,....),...], 1
return [a1,b1,...]
"""
def extract_one_dim(data,idx):
    res = []
    for d in data:
        res.append(d[idx])
    return res

"""
Given data [a0,a1,a2,...] and dow
return [a0/dow,a1/dow,...]
"""
def divide_all(data,dow):
    res = []
    for d in data:
        res.append(d / dow)
    return res

from  statistics  import mean
def compute_average(data):
    return mean(data)


import inspect
def retrieve_name(var):
    '''
    utils:
    get back the name of variables
    '''
    callers_local_vars = inspect.currentframe().f_back.f_locals.items()
    return [var_name for var_name, var_val in callers_local_vars if var_val is var]
