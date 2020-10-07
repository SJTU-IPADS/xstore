def output_gnuplot_res(file, num, *args):
    max_len = 0
    for i, j in args:
        assert(len(i) == len(j))
        max_len = max(max_len, len(i))
    max_len = min(max_len, num)
    print(max_len)

    f = open(file + ".res", "w")
    for i, j in args:
        f.write("t" + " l" + "\t")  # place holder
    f.write("\n")
    for x in range(max_len):
        for i, j in args:
            if x < len(i):
                f.write("%f %f\t" % (i[x], j[x]))
            else:
                f.write('"" ""\t')
        f.write("\n")
    f.write("\n")
    f.close()


def output_res_2(file, *args):
    config_set = set()
    for i in args:
        for k in i.keys():
            config_set.add(k)
    config_list = list(config_set)
    config_list.sort()

    f = open(file + ".res", "w")
    for _ in args:
        f.write("t" + " l" + "\t")  # place holder
    f.write("\n")
    for x in config_list:
        for i in args:
            if x in i:
                f.write("%f %f\t" % (i[x][0], i[x][1]))
            else:
                f.write('"" ""\t')
        f.write("\n")
    f.write("\n")
    f.close()

def output_aligned_lines(file,data_set,title_set):
    if len(data_set) == 0:
        return

    ## sanity check data lens
    num = len(data_set[0])
    for d in data_set:
        assert len(d) == num
    assert len(data_set) <= len(title_set)

    ## now write
    f = open(file + ".res","w")

    # first write the headers
    f.write("time\t")
    for t in title_set:
        f.write("%s\t" % t)
    f.write("\n")

    # then write datas
    for i in range(num):
        f.write("{:4.1f}\t".format(i))
        for d in data_set:
            f.write("{:10.2f}\t".format(d[i]))
        f.write("\n")
    f.close()
