import xml.etree.ElementTree as ET
import sys

# simple usage statement, hopefully helpful enough to figure
# out what's going wrong if the output isn't what's expected.
if len(sys.argv) < 3:
  sys.stderr.write("Usage: %s <input file> <output file> [<key=val>, <key=val>, ...]\n\n" % sys.argv[0])
  sys.stderr.write("E.g: %s input.xml output.xml \"*:dbname=gis\" \"ocean_layer:dbname=natural_earth\"\n\n" % sys.argv[0])
  sys.stderr.write("Each of the key-value pairs will replace parameters in the datasources in the layers in the input file, with the key being <layer name>:<parameter name>. Optionally the layer name can be '*', in which case it applies as a default when there is no more specific key.\n")
  sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]
overrides = dict()
for arg in sys.argv[3:]:
  (k, v) = arg.split('=')

  # sanity checking
  if ':' not in k:
    sys.stderr.write("Key %s does not contain a colon separating the layer name from the datasource parameter name. Keys are expected to be of the form \"<layer name>:<parameter name>\" where the layer name can optionally be the wildcard '*'.\n" % repr(k))
    sys.exit(1)

  if v is None:
    sys.stderr.write("Value for argument %s is null. The arguments should be of the form \"key=value\".\n" % repr(arg))
    sys.exit(1)

  overrides[k] = v

tree = ET.parse(input_file)
root = tree.getroot()

for layer in root.iter('Layer'):
  layer_name = layer.get('name')
  for datasource in layer.iter('Datasource'):
    for parameter in datasource.iter('Parameter'):
      p = parameter.get('name')
      layer_param = ("%s:%s" % (layer_name, p))
      generic_param = "*:%s" % p

      if layer_param in overrides:
        parameter.text = overrides[layer_param]

      elif generic_param in overrides:
        parameter.text = overrides[generic_param]

tree.write(output_file)
