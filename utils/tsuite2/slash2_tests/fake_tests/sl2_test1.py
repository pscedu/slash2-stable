def setup():
  print "setting up 1"
  return {"pass": True, "error": None}

def operate():
  print "running 1"
  return {"pass": True, "error": None}

def cleanup():
  print "cleaning up 1"
  return {"pass": True, "error": None}
