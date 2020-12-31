nclients = 5

total = 0

for i in range(nclients + 1):
    if i == 0:
        continue
    f = open(str(i) + ".xstorelog","r")
    res = []
    for line in f:
        res.append(float(line.replace(',','')))
    res = res[5:-5]
    sum = 0
    for r in res:
        sum += r
    print("add ",sum / len(res), " for:",i)
    total += sum / len(res)

print(total)
