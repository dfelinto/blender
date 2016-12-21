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

def compare_files(file_a, file_b):
    import filecmp

    if not filecmp.cmp(
        file_a,
        file_b):
        """
        import pdb
        print("Files differ:", file_a, file_b)
        pdb.set_trace()
        """
        return False

    return True


class UnitsTesting(unittest.TestCase):
    _test_simple = False

    def path_exists(self, filepath):
        import os
        self.assertTrue(
                os.path.exists(filepath),
                "Test file \"{0}\" not found".format(filepath))

    def get_root(self):
        """
        return the folder with the test files
        """
        arguments = {}
        for argument in extra_arguments:
            name, value = argument.split('=')
            self.assertTrue(name and name.startswith("--"), "Invalid argument \"{0}\"".format(argument))
            self.assertTrue(value, "Invalid argument \"{0}\"".format(argument))
            arguments[name[2:]] = value.strip('"')

        return arguments.get('testdir')

    def test__parsing(self):
        """
        Test if the arguments are properly set, and store ROOT
        name has extra _ because we need this test to run first
        """
        root = self.get_root()
        self.assertTrue(root, "Testdir not set")

    def test__import_blendfile(self):
        """
        Make sure blendfile imports with no problems
        name has extra _ because we need this test to run first
        """
        import_blendfile()
        import blendfile

    def do_scene_write_read(self, filepath_layers, filepath_layers_json, data_callbacks, do_read):
        """
        See if write/read is working for scene collections and layers
        """
        import bpy
        import os
        import tempfile
        import filecmp

        with tempfile.TemporaryDirectory() as dirpath:
            (self.path_exists(f) for f in (filepath_layers, filepath_layers_json))

            filepath_doversion = os.path.join(dirpath, 'doversion.blend')
            filepath_saved = os.path.join(dirpath, 'doversion_saved.blend')
            filepath_read_json = os.path.join(dirpath, "read.json")

            # doversion + write test
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_doversion)

            datas = query_scene(filepath_doversion, 'Main', data_callbacks)
            self.assertTrue(datas, "Data is not valid")

            filepath_doversion_json = os.path.join(dirpath, "doversion.json")
            with open(filepath_doversion_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_doversion_json,
                filepath_layers_json,
                ),
                "Run: test_scene_write_layers")

            if do_read:
                # read test, simply open and save the file
                bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_doversion)
                bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

                datas = query_scene(filepath_saved, 'Main', data_callbacks)
                self.assertTrue(datas, "Data is not valid")

                with open(filepath_read_json, "w") as f:
                    for data in datas:
                        f.write(dump(data))

                self.assertTrue(compare_files(
                    filepath_read_json,
                    filepath_layers_json,
                    ),
                    "Scene dump files differ")

    def test_scene_write_collections(self):
        """
        See if the doversion and writing are working for scene collections
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers_simple.json')

        self.do_scene_write_read(
                filepath_layers,
                filepath_layers_json,
                (get_scene_collections,),
                False)

    def test_scene_write_layers(self):
        """
        See if the doversion and writing are working for collections and layers
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers.json')

        self.do_scene_write_read(
                filepath_layers,
                filepath_layers_json,
                (get_scene_collections, get_layers),
                False)

    def test_scene_read_collections(self):
        """
        See if read is working for scene collections
        (run `test_scene_write_colections` first)
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers_simple.json')

        self.do_scene_write_read(
                filepath_layers,
                filepath_layers_json,
                (get_scene_collections,),
                True)

    def test_scene_read_layers(self):
        """
        See if read is working for scene layers
        (run `test_scene_write_layers` first)
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers.json')

        self.do_scene_write_read(
                filepath_layers,
                filepath_layers_json,
                (get_scene_collections, get_layers),
                True)

    def do_scene_copy(self, filepath_json_reference, copy_mode, data_callbacks):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            (self.path_exists(f) for f in (
                filepath_layers,
                filepath_json_reference,
                ))

            filepath_saved = os.path.join(dirpath, '{0}.blend'.format(copy_mode))
            filepath_json = os.path.join(dirpath, "{0}.json".format(copy_mode))

            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            bpy.ops.scene.new(type=copy_mode)
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

            datas = query_scene(filepath_saved, 'Main.001', data_callbacks)
            self.assertTrue(datas, "Data is not valid")

            with open(filepath_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_json,
                filepath_json_reference,
                ),
                "Scene copy \"{0}\" test failed".format(copy_mode.title()))

    def test_scene_collections_copy_full(self):
        """
        See if scene copying 'FULL_COPY' is working for scene collections
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_full_simple.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'FULL_COPY',
                (get_scene_collections,))

    def test_scene_collections_link(self):
        """
        See if scene copying 'LINK_OBJECTS' is working for scene collections
        """
        import os
        ROOT = self.get_root()

        # note: nothing should change, so using `layers_simple.json`
        filepath_layers_json_copy = os.path.join(ROOT, 'layers_simple.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'LINK_OBJECTS',
                (get_scene_collections,))

    def test_scene_layers_copy(self):
        """
        See if scene copying 'FULL_COPY' is working for scene layers
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_full.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'FULL_COPY',
                (get_scene_collections, get_layers))

    def test_scene_layers_link(self):
        """
        See if scene copying 'FULL_COPY' is working for scene layers
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_link.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'LINK_OBJECTS',
                (get_scene_collections, get_layers))

    def do_syncing(self, filepath_json, do_unlink):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')
            three_d = bpy.data.objects.get('T.3d')

            scene = bpy.context.scene

            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = scene.master_collection.collections['1'].collections.new('scorpion')

            # test linking sync
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)

            if do_unlink:
                # test unlinking sync
                scorpion.objects.link(three_d)
                scorpion.objects.unlink(three_d)

            # save file
            filepath_nested = os.path.join(dirpath, 'nested.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_nested)

            # get the generated json
            datas = query_scene(filepath_nested, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_nested_json = os.path.join(dirpath, "nested.json")
            with open(filepath_nested_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_nested_json,
                filepath_json,
                ),
                "Scene dump files differ")

    def test_syncing_link(self):
        """
        See if scene collections and layer collections are in sync
        when we create new subcollections and link new objects
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_nested.json')
        self.do_syncing(filepath_json, False)

    def test_syncing_unlink(self):
        """
        See if scene collections and layer collections are in sync
        when we create new subcollections, link new objects and unlink
        some.
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_nested.json')
        self.do_syncing(filepath_json, True)


# ############################################################
# Main
# ############################################################

if __name__ == '__main__':
    import sys

    global extra_arguments
    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])
    unittest.main()
