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
