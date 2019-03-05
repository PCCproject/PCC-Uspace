import shim

shim.init(1)
shim.reset(1)
shim.give_sample(1, 1.0, 0.99, 0.033, 0.00, 0.01, 0.66)
print("Got rate: %f" % shim.get_rate(1))
