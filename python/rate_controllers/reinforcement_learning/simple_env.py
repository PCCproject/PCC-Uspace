import loaded_client as lc

lc.init(0)
lc.reset(0)
lc.give_sample(0, 200, 20, 20, 0.0, 0.06, 0)
print(lc.get_rate(0))
