
import subprocess
from os.path import join, dirname

valid_outputs = [
"""\
try
after try
""",

"""\
try
else
finally
""",

"""\
try
except: Invalid argument value.
finally
""",

"""\
runner: before
inner try
inner finally
except: Invalid argument value.
finally
runner: except: Invalid argument value.
""",

"""\
runner: before
except: Invalid argument value.
finally
runner: except: Unexpected or internal error.
""",
]


def check_output(test_no):
	executable = join(dirname(__file__), "test_exception")
	p = subprocess.Popen(
			args=[executable, str(test_no)],
			stdout=subprocess.PIPE,
			)
	output = p.communicate()[0]
	assert output == valid_outputs[test_no]

def test_exception():
	for i in range(5):
		yield check_output, i

