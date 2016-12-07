# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_render_layer.py -- --testdir="/data/lib/tests/"
import unittest


# ############################################################
# Layer Collection Crawler
# ############################################################

def listbase_iter(data, struct, listbase):
    element = data.get_pointer((struct, listbase, b'first'))
    while element is not None:
        yield element
        element = element.get_pointer(b'next')


def linkdata_iter(collection, data):
    element = collection.get_pointer((data, b'first'))
    while element is not None:
        yield element
        element = element.get_pointer(b'next')


def get_layer_collection(layer_collection):
    data = {}
    flag = layer_collection.get(b'flag')

    data['is_visible']    = (flag & (1 << 0)) != 0;
    data['is_selectable'] = (flag & (1 << 1)) != 0;
    data['is_folded']     = (flag & (1 << 2)) != 0;

    scene_collection = layer_collection.get_pointer(b'scene_collection')
    if scene_collection is None:
        name = 'Fail!'
    else:
        name = scene_collection.get(b'name')
    data['name'] = name

    objects = []
    for link in linkdata_iter(layer_collection, b'object_bases'):
        ob_base = link.get_pointer(b'data')
        ob = ob_base.get_pointer(b'object')
        objects.append(ob.get((b'id', b'name'))[2:])
    data['objects'] = objects

    collections = {}
    for nested_layer_collection in linkdata_iter(layer_collection, b'layer_collections'):
        subname, subdata = get_layer_collection(nested_layer_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_layer(layer):
    data = {}
    name = layer.get(b'name')

    data['name'] = name
    data['active_object'] = layer.get((b'basact', b'object', b'id', b'name'))[2:]
    data['engine'] = layer.get(b'engine')

    objects = []
    for link in linkdata_iter(layer, b'object_bases'):
        ob = link.get_pointer(b'object')
        objects.append(ob.get((b'id', b'name'))[2:])
    data['objects'] = objects

    collections = {}
    for layer_collection in linkdata_iter(layer, b'layer_collections'):
        subname, subdata = get_layer_collection(layer_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_layers(scene):
    """Return all the render layers and their data"""
    layers = {}
    for layer in linkdata_iter(scene, b'render_layers'):
        name, data = get_layer(layer)
        layers[name] = data
    return layers


def get_scene_collection_objects(collection, listbase):
    objects = []
    for link in linkdata_iter(collection, listbase):
        ob = link.get_pointer(b'data')
        if ob is None:
            name  = 'Fail!'
        else:
            name = ob.get((b'id', b'name'))[2:]
        objects.append(name)
    return objects


def get_scene_collection(collection):
    """"""
    data = {}
    name = collection.get(b'name')

    data['name'] = name
    data['filter'] = collection.get(b'filter')

    data['objects'] = get_scene_collection_objects(collection, b'objects')
    data['filter_objects'] = get_scene_collection_objects(collection, b'filter_objects')

    collections = {}
    for nested_collection in linkdata_iter(collection, b'scene_collections'):
        subname, subdata = get_scene_collection(nested_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_scene_collections(scene):
    """Return all the scene collections ahd their data"""
    master_collection = scene.get_pointer(b'collection')
    return get_scene_collection(master_collection)


def query_scene(filepath, name, callbacks):
    """Return the equivalent to bpy.context.scene"""
    import blendfile
    with blendfile.open_blend(filepath) as blend:
        scenes = [block for block in blend.blocks if block.code == b'SC']
        for scene in scenes:
            if scene.get((b'id', b'name'))[2:] == name:
                output = []
                for callback in callbacks:
                    output.append(callback(scene))
                return output


# ############################################################
# Utils
# ############################################################

def import_blendfile():
    import bpy
    import os, sys
    path = os.path.join(
            bpy.utils.resource_path('LOCAL'),
            'scripts',
            'addons',
            'io_blend_utils',
            'blend',
            )

    if path not in sys.path:
        sys.path.append(path)


def dump(data):
    import json
    return json.dumps(
            data,
            sort_keys=True,
            indent=4,
            separators=(',', ': '),
            )


# ############################################################
# Tests
# ############################################################

class UnitsTesting(unittest.TestCase):

    def path_exists(self, filepath):
        import os
        self.assertTrue(
                os.path.exists(filepath),
                "Test file \"{0}\" not found".format(filepath))

    def test_parsing(self):
        """Test if the arguments are properly set, and store ROOT"""

        arguments = {}
        for argument in extra_arguments:
            name, value = argument.split('=')
            self.assertTrue(name and name.startswith("--"), "Invalid argument \"{0}\"".format(argument))
            self.assertTrue(value, "Invalid argument \"{0}\"".format(argument))
            arguments[name[2:]] = value.strip('"')

        global ROOT
        ROOT = arguments.get('testdir')

        self.assertTrue(ROOT, "Testdir not set")

    def test_import_blendfile(self):
        """Make sure blendfile imports with no problems"""
        import_blendfile()
        import blendfile

    def test_scene_doversion(self):
        """See if the doversion, writing and reading is working well"""
        import bpy
        import os
        import tempfile
        import filecmp

        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')
            filepath_layers_json = os.path.join(ROOT, 'layers.json')

            (self.path_exists(f) for f in (filepath_layers, filepath_layers_json))

            # doversion + write test
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            filepath_doversion = os.path.join(dirpath, 'doversion.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_doversion)

            data = query_scene(filepath_doversion, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(data, "Data is not valid")
            collections, layers = data

            filepath_doversion_json = os.path.join(dirpath, "doversion.json")
            with open(filepath_doversion_json, "w") as f:
                f.write(dump(collections))
                f.write(dump(layers))

            self.assertTrue(filecmp.cmp(
                filepath_doversion_json,
                filepath_layers_json),
                "Doversion test failed")

            # read test
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_doversion)
            filepath_saved = os.path.join(dirpath, 'doversion_saved.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

            data = query_scene(filepath_saved, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(data, "Data is not valid")
            collections, layers = data

            filepath_read_json = os.path.join(dirpath, "read.json")
            with open(filepath_read_json, "w") as f:
                f.write(dump(collections))
                f.write(dump(layers))

            self.assertTrue(filecmp.cmp(
                filepath_read_json,
                filepath_layers_json),
                "Read test failed")

    def test_scene_copy(self):
        import bpy
        import os
        import tempfile
        import filecmp

        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')
            filepath_layers_json = os.path.join(ROOT, 'layers.json')
            filepath_layers_copy_json = os.path.join(ROOT, 'layers_copy.json')

            (self.path_exists(f) for f in (
                filepath_layers, filepath_layers_json, filepath_layers_copy_json))

            type_lookup = {
                    'LINK_OBJECTS': filepath_layers_json,
                    'FULL_COPY': filepath_layers_copy_json,
                    }

            for scene_type, json_reference_file in type_lookup.items():
                bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
                bpy.ops.scene.new(type=scene_type)

                filepath_saved = os.path.join(dirpath, '{0}.blend'.format(scene_type))
                bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

                data = query_scene(filepath_saved, 'Main.001', (get_scene_collections, get_layers))
                self.assertTrue(data, "Data is not valid")
                collections, layers = data

                filepath_json = os.path.join(dirpath, "{0}.json".format(scene_type))
                with open(filepath_json, "w") as f:
                    f.write(dump(collections))
                    f.write(dump(layers))

                self.assertTrue(filecmp.cmp(
                    json_reference_file,
                    filepath_json),
                    "Scene copy \"{0}\" test failed".format(scene_type.title()))


# ############################################################
# Main
# ############################################################

if __name__ == '__main__':
    import sys

    global extra_arguments
    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])
    unittest.main()
