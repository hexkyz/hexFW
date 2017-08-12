import ConfigParser, hashlib

cfg = ConfigParser.ConfigParser()
cfg.read("../Keys.txt")

wiiu_common_key=cfg.get("KEYS", "wii_u_common_key")
ancast_key=cfg.get("KEYS", "starbuck_ancast_key")
ancast_iv=cfg.get("KEYS", "starbuck_ancast_iv")

print("Verifying keys...");

if hashlib.md5(wiiu_common_key).hexdigest() != "35ac5994972279331d97094fa2fb97fc":
	print("Invalid Wii U common key.\nExpected MD5: 35ac5994972279331d97094fa2fb97fc\nGot: "+hashlib.md5(wiiu_common_key).hexdigest())
	exit(1)

if hashlib.md5(ancast_key).hexdigest() != "318d1f9d98fb08e77c7fe177aa490543":
	print("Invalid Starbuck ancast key.\nExpected MD5: 318d1f9d98fb08e77c7fe177aa490543\nGot: "+hashlib.md5(ancast_key).hexdigest())
	exit(1)

if hashlib.md5(ancast_iv).hexdigest() != "5cf3043b56e8cda7e454888124e74d54":
	print("Invalid Starbuck ancast IV.\nExpected MD5: 5cf3043b56e8cda7e454888124e74d54\nGot: "+hashlib.md5(ancast_iv).hexdigest())
	exit(1)

print("All keys are valid.");

exit(0)
