# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy
import bmesh
from bpy_extras import view3d_utils, object_utils

import math
import os
import blf
import bgl
from bpy.app.translations import pgettext_iface as iface_ #for decimate modifier

from bpy.types import PropertyGroup

from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       FloatVectorProperty,
                       BoolVectorProperty,
                       PointerProperty,
                       CollectionProperty,
                       EnumProperty)

class const():
    icon_info = 'INFO'
    icon_display = 'RESTRICT_VIEW_OFF'
    icon_mesh_data = 'MESH_DATA'
    icon_curve_data = 'CURVE_DATA'
    icon_empty_data = 'EMPTY_DATA'
    icon_font_data = 'FONT_DATA'
    icon_light = 'LAMP_SPOT'
    icon_camera = 'OUTLINER_DATA_CAMERA'
    icon_library = 'EXTERNAL_DATA'
    icon_project = 'RENDERLAYERS'
    icon_wall = 'SNAP_PEEL_OBJECT'
    icon_product = 'OBJECT_DATAMODE'
    icon_insert = 'STICKY_UVS_LOC'
    icon_part = 'MOD_MESHDEFORM'
    icon_object = 'OBJECT_DATA'
    icon_group = 'GROUP'
    icon_scene = 'SCENE_DATA'
    icon_world = 'WORLD'
    icon_mesh = 'OUTLINER_OB_MESH'
    icon_font = 'OUTLINER_OB_FONT'
    icon_empty = 'OUTLINER_OB_EMPTY'
    icon_material = 'MATERIAL'
    icon_curve = 'OUTLINER_OB_CURVE'
    icon_extrusion = 'CURVE_PATH'
    icon_properties = 'INFO'
    icon_drivers = 'AUTO'
    icon_modifiers = 'MODIFIER'
    icon_constraints = 'CONSTRAINT'
    icon_category = 'FILE_FOLDER'
    icon_ghost = 'GHOST_ENABLED'
    icon_editmode = 'EDITMODE_HLT'
    icon_hook = 'HOOK'
    icon_edit_text = 'OUTLINER_DATA_FONT'
    icon_specgroup = 'SOLO_ON'
    icon_filefolder = 'FILE_FOLDER'

    #ACTIONS
    icon_add = 'ZOOM_IN'
    icon_delete = 'ZOOM_OUT'
    icon_refresh = 'FILE_REFRESH'

class enums():
    enum_object_tabs = [('INFO',"","Show the Main Information"),
                        ('DISPLAY',"","Show Options for how the Object is Displayed"),
                        ('MATERIAL',"","Show the materials assign to the object"),
                        ('CONSTRAINTS',"","Show the constraints assigned to the object"),
                        ('MODIFIERS',"","Show the modifiers assigned to the object"),
                        ('MESHDATA',"","Show the Mesh Data Information"),
                        ('CURVEDATA',"","Show the Curve Data Information"),
                        ('TEXTDATA',"","Show the Text Data Information"),
                        ('EMPTYDATA',"","Show the Empty Data Information"),
                        ('LIGHTDATA',"","Show the Light Data Information"),
                        ('CAMERADATA',"","Show the Camera Data Information"),
                        ('DRIVERS',"","Show the Drivers assigned to the Object")]

    enum_group_tabs = [('INFO',"Main","Show the Main Info Page"),
                       ('SETTINGS',"","Show the Settings Page"),
                       ('PROMPTS',"Prompts","Show the Prompts Page"),
                       ('OBJECTS',"Objects","Show Objects"),
                       ('DRIVERS',"Drivers","Show the Driver Formulas")]
    
    enum_calculator_type = [('XDIM',"X Dimension","Calculate the X Dimension"),
                            ('YDIM',"Y Dimension","Calculate the Y Dimension"),
                            ('ZDIM',"Z Dimension","Calculate the Z Dimension")]
    
    enum_prompt_tab_types = [('VISIBLE',"Visible","Visible tabs are always displayed"),
                             ('HIDDEN',"Hidden","Hidden tabs are not shown in the right click menu"),
                             ('CALCULATOR',"Calculator","Use to calculate sizes of opening")]
    
    enum_prompt_types = [('NUMBER',"Number","Number"),
                         ('QUANTITY',"Quantity","Quantity"),
                         ('COMBOBOX',"Combo Box","Combo Box"),
                         ('CHECKBOX',"Check Box","Check Box"),
                         ('TEXT',"Text","Text"),
                         ('DISTANCE',"Distance","Distance"),
                         ('ANGLE',"Angle","Angle"),
                         ('PERCENTAGE',"Percentage","Percentage"),
                         ('PRICE',"Price","Enter Price Prompt")]

    enum_object_types = [('NONE',"None","None"),
                         ('CAGE',"CAGE","Cage used to represent the bounding area of an assembly"),
                         ('VPDIMX',"Visible Prompt X Dimension","Visible prompt control in the 3D viewport"),
                         ('VPDIMY',"Visible Prompt Y Dimension","Visible prompt control in the 3D viewport"),
                         ('VPDIMZ',"Visible Prompt Z Dimension","Visible prompt control in the 3D viewport"),
                         ('EMPTY1',"Empty1","EMPTY1"),
                         ('EMPTY2',"Empty2","EMPTY2"),
                         ('EMPTY3',"Empty3","EMPTY3"),
                         ('BPWALL',"Wall Base Point","Parent object of a wall"),
                         ('BPASSEMBLY',"Base Point","Parent object of an assembly")]
    
    enum_group_drivers_tabs = [('LOC_X',"Location X","Location X"),
                               ('LOC_Y',"Location Y","Location Y"),
                               ('LOC_Z',"Location Z","Location Z"),
                               ('ROT_X',"Rotation X","Rotation X"),
                               ('ROT_Y',"Rotation Y","Rotation Y"),
                               ('ROT_Z',"Rotation Z","Rotation Z"),
                               ('DIM_X',"Dimension X","Dimension X"),
                               ('DIM_Y',"Dimension Y","Dimension Y"),
                               ('DIM_Z',"Dimension Z","Dimension Z"),
                               ('PROMPTS',"Prompts","Prompts")]

    enum_render_type = [('PRR','Photo Realistic Render','Photo Realistic Render'),
                        ('NPR','Line Drawing','Non-Photo Realistic Render')]
    
    enum_ab_visual_dimension_placement =[('ABOVE',"Above","Place Visual Dimension Above"),
                                         ('BELOW',"Below","Place Visual Dimension Below")]
    
    enum_lr_visual_dimension_placement =[('LEFT',"Left","Place Visual Dimension Left"),
                                         ('RIGHT',"Right","Place Visual Dimension Right")]    
    
    enum_visual_dimensions = [('X',"X Dimension","Draw X Dimension"),
                              #('Y',"Y Dimension","Draw Y Dimension"),
                              ('Z',"Z Dimension","Draw Z Dimension")]        
    
    enum_visual_dimension_arrows = [('Arrow1', 'Arrow1', 'Arrow1'), 
                                    ('Arrow2', 'Arrow2', 'Arrow2'), 
                                    ('Serifs1', 'Serifs1', 'Serifs1'), 
                                    ('Serifs2', 'Serifs2', 'Serifs2'), 
                                    ('Without', 'Without', 'Without')]    
    
    enum_modifiers = [('ARRAY', "Array Modifier", "Array Modifier", 'MOD_ARRAY', 0),
                      ('BEVEL', "Bevel Modifier", "Bevel Modifier", 'MOD_BEVEL', 1),
                      ('BOOLEAN', "Boolean Modifier", "Boolean Modifier", 'MOD_BOOLEAN', 2),
                      ('CURVE', "Curve Modifier", "Curve Modifier", 'MOD_CURVE', 3),
                      ('DECIMATE', "Decimate Modifier", "Decimate Modifier", 'MOD_DECIM', 4),
                      ('EDGE_SPLIT', "Edge Split Modifier", "Edge Split Modifier", 'MOD_EDGESPLIT', 5),
                      ('HOOK', "Hook Modifier", "Hook Modifier", 'HOOK', 6),
                      ('MASK', "Mask Modifier", "Mask Modifier", 'MOD_MASK', 7),
                      ('MIRROR', "Mirror Modifier", "Mirror Modifier", 'MOD_MIRROR', 8),
                      ('SOLIDIFY', "Solidify Modifier", "Solidify Modifier", 'MOD_SOLIDIFY', 9),
                      ('SUBSURF', "Subsurf Modifier", "Subsurf Modifier", 'MOD_SUBSURF', 10),
                      ('SKIN', "Skin Modifier", "Skin Modifier", 'MOD_SKIN', 11),
                      ('TRIANGULATE', "Triangulate Modifier", "Triangulate Modifier", 'MOD_TRIANGULATE', 12),
                      ('WIREFRAME', "Wireframe Modifier", "Wireframe Modifier", 'MOD_WIREFRAME', 13)]
    
    enum_constraints = [('COPY_LOCATION', "Copy Location", "Copy Location", 'CONSTRAINT_DATA', 0),
                        ('COPY_ROTATION', "Copy Rotation", "Copy Rotation", 'CONSTRAINT_DATA', 1),
                        ('COPY_SCALE', "Copy Scale", "Copy Scale", 'CONSTRAINT_DATA', 2),
                        ('COPY_TRANSFORMS', "Copy Transforms", "Copy Transforms", 'CONSTRAINT_DATA', 3),
                        ('LIMIT_DISTANCE', "Limit Distance", "Limit Distance", 'CONSTRAINT_DATA', 4),
                        ('LIMIT_LOCATION', "Limit Location", "Limit Location", 'CONSTRAINT_DATA', 5),
                        ('LIMIT_ROTATION', "Limit Rotation", "Limit Rotation", 'CONSTRAINT_DATA', 6),
                        ('LIMIT_SCALE', "Limit Scale", "Limit Scale", 'CONSTRAINT_DATA', 7)]
    
class fd_tab(bpy.types.PropertyGroup):
    type = EnumProperty(name="Type",items=enums.enum_prompt_tab_types,default='VISIBLE')
    index = IntProperty(name="Index")
    calculator_type = EnumProperty(name="Calculator Type",items=enums.enum_calculator_type)
    calculator_deduction = FloatProperty(name="Calculator Deduction",subtype='DISTANCE')
    
bpy.utils.register_class(fd_tab)
    
class fd_tab_col(bpy.types.PropertyGroup):
    col_tab = CollectionProperty(name="Collection Tab",type=fd_tab)
    index_tab = IntProperty(name="Index Tab",min=-1)

    def add_tab(self, name):
        tab = self.col_tab.add()
        tab.name = name
        tab.index = len(self.col_tab)
        return tab
    
    def delete_tab(self, index):
        for index, tab in enumerate(self.col_tab):
            if tab.index == index:
                self.col_tab.remove(index)

    def draw_tabs(self,layout):
        layout.template_list("FD_UL_specgroups", " ", self, "col_tab", self, "index_tab", rows=3)

bpy.utils.register_class(fd_tab_col)

#TODO: implement the standard collections or remove this and add to RNA Structure
class mvPrompt(bpy.types.PropertyGroup):
    Type = EnumProperty(name="Type",items=enums.enum_prompt_types)
    TabIndex = IntProperty(name="Tab Index",default = 0)
    lock_value = BoolProperty(name="lock value")
    equal = BoolProperty(name="Equal",description="Used in calculators to calculate remaining size")
    columns = IntProperty(name="Columns",description="Used for Combo Boxes to determine how many columns should be displayed",min=1,default=1)
    COL_EnumItem = bpy.props.CollectionProperty(name="COL_Enum Items",type=fd_tab)
    EnumIndex = IntProperty(name="EnumIndex")
    
    CheckBoxValue = BoolProperty(name="Check Box Values")
    
    QuantityValue = IntProperty(name="Quantity Value",min=0)
    
    TextValue = StringProperty(name="Text Value")
    
    NumberValue = FloatProperty(name="Number Value",precision=4,step=10)
    
    DistanceValue = FloatProperty(name="Distance Value",precision=4,step=10,subtype='DISTANCE')
    
    AngleValue = FloatProperty(name="Rotation Value",subtype='ANGLE')
    
    PriceValue = FloatProperty(name="Price Value",precision=2,step=10)
    
    PercentageValue = FloatProperty(name="Percentage Value",subtype='PERCENTAGE',min=0,max=1)
    
    TypeName = {'NUMBER':"NumberValue",'QUANTITY':"QuantityValue",'COMBOBOX':"EnumIndex",\
                'CHECKBOX':"CheckBoxValue",'TEXT':"TextValue",'DISTANCE':"DistanceValue",\
                'ANGLE':"AngleValue",'PERCENTAGE':"PercentageValue",'PRICE':"PriceValue"} #DELETE
    
    def draw_prompt(self,layout,data,allow_edit=True,text="",split_text=True):
        data_type = 'OBJECT' #SETS DEFAULT
        
        if data is bpy.types.Scene:
            data_type = 'SCENE'
        elif data is bpy.types.Material:
            data_type = 'MATERIAL'
        elif data is bpy.types.World:
            data_type = 'WORLD'
        
        row = layout.row()
        if text != "":
            prompt_text = text
        else:
            prompt_text = self.name
        
        if split_text:
            row.label(prompt_text)
            prompt_text = ""
            
        if self.Type == 'NUMBER':
            if self.lock_value:
                row.label(str(self.NumberValue))
            else:
                row.prop(self,"NumberValue",text=prompt_text)
            
        if self.Type == 'ANGLE':
            if self.lock_value:
                row.label(str(self.AngleValue))
            else:
                row.prop(self,"AngleValue",text=prompt_text)
            
        if self.Type == 'DISTANCE':
            if self.lock_value:
                row.label(str(self.DistanceValue))
            else:
                row.prop(self,"DistanceValue",text=prompt_text)
            
        if self.Type == 'PERCENTAGE':
            if self.lock_value:
                row.label(str(self.PercentageValue))
            else:
                row.prop(self,"PercentageValue",text=prompt_text)
            
        if self.Type == 'PRICE':
            if self.lock_value:
                row.label(str(self.PriceValue))
            else:
                row.prop(self,"PriceValue",text=prompt_text)
            
        if self.Type == 'QUANTITY':
            if self.lock_value:
                row.label(str(self.QuantityValue))
            else:
                row.prop(self,"QuantityValue",text=prompt_text)
            
        if self.Type == 'COMBOBOX':
            if self.lock_value:
                row.label(self.COL_EnumItem[self.EnumIndex].name)
            else:
                if allow_edit:
                    prop = row.operator("fd_prompts.add_combo_box_option",icon='ZOOMIN',text="")
                    prop.prompt_name = self.name
                    prop.data_name = data.name
                col = layout.column()
                col.template_list("FD_UL_combobox"," ", self, "COL_EnumItem", self, "EnumIndex",rows=len(self.COL_EnumItem)/self.columns,type='GRID',columns=self.columns)
        
        if self.Type == 'CHECKBOX':
            if self.lock_value:
                row.label(str(self.CheckBoxValue))
            else:
                row.prop(self,"CheckBoxValue",text=prompt_text)
            
        if self.Type == 'SLIDER':
            row.prop(self,"NumberValue",text=prompt_text,slider=True)
            
        if self.Type == 'TEXT':
            row.prop(self,"TextValue",text=prompt_text)
            
        if allow_edit:
            props = row.operator("fd_prompts.show_prompt_properties",text="",icon='INFO')
            props.prompt_name = self.name
            props.data_type = data_type
            props.data_name = data.name
            
            props = row.operator("fd_prompts.delete_prompt",icon='X',text="")
            props.prompt_name = self.name
            props.data_type = data_type
            props.data_name = data.name
        
    def draw_calculation_prompt(self,layout,data,allow_edit=True):
        row = layout.row()
        row.label(self.name)
        if self.equal:
            row.label(str(unit(self.DistanceValue)))
            row.prop(self,'equal',text='')
        else:
            row.prop(self,'DistanceValue',text="")
            row.prop(self,'equal',text='')
        
        props = row.operator("fd_prompts.delete_prompt",icon='X',text="")
        props.prompt_name = self.name
        props.data_type = 'OBJECT'
        props.data_name = data.name
        
    def add_enum_item(self,Name):
        Item = self.COL_EnumItem.add()
        Item.name = Name
        
    def Update(self):
        self.NumberValue = self.NumberValue
        self.TextValue = self.TextValue
        self.QuantityValue = self.QuantityValue
        self.CheckBoxValue = self.CheckBoxValue
        self.EnumIndex = self.EnumIndex
        
    def draw_prompt_properties(self,layout,object_name):
        row = layout.row()
        row.label(self.name)
        props = row.operator('fd_prompts.rename_prompt',text="Rename Prompt",icon='GREASEPENCIL')
        props.data_name = object_name
        props.old_name = self.name
        row = layout.row()
        row.label('Type:')
        row.prop(self,"Type",text="")
        typeUpdate = row.operator('fd_prompts.update_formulas_w_new_prompt_type',text="",icon='FILE_REFRESH')
        typeUpdate.data_name = object_name
        typeUpdate.name = self.name
        row = layout.row()
        row.label('Lock Value:')
        row.prop(self,"lock_value",text="")
        row = layout.row()
        row.label('Tab Index:')
        row.prop(self,"TabIndex",expand=True,text="")
        if self.Type == 'COMBOBOX':
            row = layout.row()
            row.label('Columns:')
            row.prop(self,"columns",text="")
        
    def show_prompt_tabs(self,layout):
        layout.template_list("FD_UL_prompttabs"," ", self, "COL_MainTab", self, "MainTabIndex",rows=len(self.COL_MainTab),type='DEFAULT')
        
    def value(self):
        if self.Type == 'NUMBER':
            return self.NumberValue
            
        if self.Type == 'ANGLE':
            return self.AngleValue
            
        if self.Type == 'DISTANCE':
            return self.DistanceValue
            
        if self.Type == 'PERCENTAGE':
            return self.PercentageValue
            
        if self.Type == 'PRICE':
            return self.PriceValue
            
        if self.Type == 'QUANTITY':
            return self.QuantityValue
            
        if self.Type == 'COMBOBOX':
            return self.COL_EnumItem[self.EnumIndex]

        if self.Type == 'CHECKBOX':
            return self.CheckBoxValue
            
        if self.Type == 'SLIDER':
            return self.NumberValue
        
        if self.Type == 'TEXT':
            return self.TextValue
        
    def set_value(self,value):
        if self.Type == 'NUMBER':
            self.NumberValue = value
            
        if self.Type == 'ANGLE':
            self.AngleValue = value
            
        if self.Type == 'DISTANCE':
            self.DistanceValue = value
            
        if self.Type == 'PERCENTAGE':
            self.PercentageValue = value
            
        if self.Type == 'PRICE':
            self.PriceValue = value
            
        if self.Type == 'QUANTITY':
            self.QuantityValue = value
            
        if self.Type == 'COMBOBOX':
            for index, item in enumerate(self.COL_EnumItem):
                if item.name == value:
                    self.EnumIndex = index

        if self.Type == 'CHECKBOX':
            self.CheckBoxValue = value
            
        if self.Type == 'SLIDER':
            self.NumberValue = value
        
        if self.Type == 'TEXT':
            self.TextValue = value
        
    def get_type_as_string(self, type):
        return self.TypeName[type]
        
bpy.utils.register_class(mvPrompt)

#TODO: implement the standard collections or remove this and add to RNA Structure
class mvPromptPage(bpy.types.PropertyGroup):
    COL_MainTab = CollectionProperty(name="COL_MainTab",type=fd_tab)
    MainTabIndex = IntProperty(name="Main Tab Index")
    COL_Prompt = CollectionProperty(name="COL_Prompt",type=mvPrompt)
    PromptIndex = IntProperty(name="Prompt Index")

    def add_tab(self,Name,tab_type):
        Tab = self.COL_MainTab.add()
        Tab.name = Name
        Tab.type = tab_type
    
    def add_prompt(self,name,type,data_name):
        Prompt = self.COL_Prompt.add()
        Prompt.name = name
        Prompt.Type = type
        return Prompt

    def delete_prompt(self,Name):
        for index, Prompt in enumerate(self.COL_Prompt):
            if Prompt.name == Name:
                self.COL_Prompt.remove(index)

    def delete_selected_tab(self):
        self.COL_MainTab.remove(self.MainTabIndex)

    def rename_selected_tab(self,Name):
        self.COL_MainTab[self.MainTabIndex].name = Name

    #TODO: MAYBE DELETE THIS
    def Update(self):
        for Prompt in self.COL_Prompt:
            Prompt.Update()

    def run_calculators(self,obj):
        for index, tab in enumerate(obj.mv.PromptPage.COL_MainTab):
            if tab.type == 'CALCULATOR':
                bpy.ops.fd_prompts.run_calculator(data_name=obj.name,tab_index=index)

    def draw_prompts_list(self,layout):
        Rows = len(self.COL_Prompt)
        if Rows > 8:
            Rows = 10
        layout.template_list("MV_UL_default"," ", self, "COL_Prompt", self, "PromptIndex",rows=Rows)
        Prompt = self.COL_Prompt[self.PromptIndex]
        Prompt.DrawPrompt(layout,obj=None,AllowEdit=False)

    def draw_prompt_page(self,layout,data,allow_edit=True):
        datatype = 'OBJECT'
        if type(data) is bpy.types.Scene:
            datatype = 'SCENE'
        elif type(data) is bpy.types.Material:
            datatype = 'MATERIAL'
        elif type(data) is bpy.types.World:
            datatype = 'WORLD'
        row = layout.row(align=True)
        if allow_edit:
            props = row.operator("fd_prompts.add_main_tab",text="Add Tab",icon='SPLITSCREEN')
            props.data_type = datatype
            props.data_name = data.name
        if len(self.COL_MainTab) > 0:
            if allow_edit:
                props1 = row.operator("fd_prompts.rename_main_tab",text="Rename Tab",icon='GREASEPENCIL')
                props1.data_type = datatype
                props1.data_name = data.name
                props2 = row.operator("fd_prompts.delete_main_tab",text="Delete Tab",icon='X')
                props2.data_type = datatype
                props2.data_name = data.name
                
            layout.template_list("FD_UL_prompttabs"," ", self, "COL_MainTab", self, "MainTabIndex",rows=len(self.COL_MainTab),type='DEFAULT')
            box = layout.box()
            tab = self.COL_MainTab[self.MainTabIndex]
            if tab.type == 'CALCULATOR':
                row = box.row()
                row.prop(tab,'calculator_type',text='')
                row.prop(tab,'calculator_deduction',text='Deduction')
                props3 = box.operator("fd_prompts.add_calculation_prompt",text="Add Calculation Variable",icon='UI')
                props3.data_type = datatype
                props3.data_name = data.name
                for prompt in self.COL_Prompt:
                    if prompt.TabIndex == self.MainTabIndex:
                        prompt.draw_calculation_prompt(box,data,allow_edit)
                props3 = box.operator("fd_prompts.run_calculator",text="Calculate")
                props3.data_type = datatype
                props3.data_name = data.name
                props3.tab_index = self.MainTabIndex
            else:
                props3 = box.operator("fd_prompts.add_prompt",text="Add Prompt",icon='UI')
                props3.data_type = datatype
                props3.data_name = data.name
                for prompt in self.COL_Prompt:
                    if prompt.TabIndex == self.MainTabIndex:
                        prompt.draw_prompt(box,data,allow_edit)

    def show_prompts(self,layout,obj,index,tab_name=""):
        #If the tab_name is passed in set the index
        if tab_name != "":
            for i, tab in enumerate(self.COL_MainTab):
                if tab.name == tab_name:
                    index = i
                    break
        
        tab = self.COL_MainTab[index]
        if tab.type == 'CALCULATOR':
            for Prompt in self.COL_Prompt:
                if Prompt.TabIndex == index:
                    row = layout.row()
                    row.label(Prompt.name)
                    row.prop(Prompt,"DistanceValue",text="")
                    row.prop(Prompt,"equal",text="")
        else:
            for Prompt in self.COL_Prompt:
                if Prompt.TabIndex == index:
                    Prompt.draw_prompt(layout,obj,allow_edit=False)
                    
bpy.utils.register_class(mvPromptPage)

class fd_object(PropertyGroup):

    type = EnumProperty(name="type",
                        items=enums.enum_object_types,
                        description="Select the Object Type.",
                        default='NONE')

    property_id = StringProperty(name="Property ID",
                                 description="This property allows objects to display a custom property page. This is the operator bl_id.")

    name_object = StringProperty(name="Object Name",
                                 description="This is the readable name of the object")
    
    use_as_mesh_hook = BoolProperty(name="Use As Mesh Hook",
                                    description="Use this object to hook to deform a mesh. Only for Empties",
                                    default=False)
    
    use_as_bool_obj = BoolProperty(name="Use As Boolean Object",
                                   description="Use this object cut a hole in the selected mesh",
                                   default=False)

    PromptPage = bpy.props.PointerProperty(name="Prompt Page",
                                           description="Custom properties assigned to the object. Only access from base point object.",
                                           type=mvPromptPage)

bpy.utils.register_class(fd_object)

class fd_interface(PropertyGroup):
    
    show_debug_tools = BoolProperty(name="Show Debug Tools",
                                    default = False,
                                    description="Show Debug Tools")
    
    use_default_blender_interface = BoolProperty(name="Use Default Blender Interface",
                                                 default = False,
                                                 description="Show Default Blender interface")
    
    interface_object_tabs = EnumProperty(name="Interface Object Tabs",
                                         items=enums.enum_object_tabs,
                                         default = 'INFO')
    
    interface_group_tabs = EnumProperty(name="Interface Group Tabs",
                                        items=enums.enum_group_tabs
                                        ,default = 'INFO')
    
    group_tabs = EnumProperty(name="Group Tabs",
                              items=enums.enum_group_tabs,
                              description="Group Tabs",
                              default='INFO')
    
    group_driver_tabs = EnumProperty(name="Group Driver Tabs",
                                     items=enums.enum_group_drivers_tabs,
                                     default = 'LOC_X')

    render_type_tabs = EnumProperty(name="Render Type Tabs",
                                    items=enums.enum_render_type,
                                    default='PRR')
    
bpy.utils.register_class(fd_interface)

class fd_item(PropertyGroup):
    pass

bpy.utils.register_class(fd_item)

class fd_scene(PropertyGroup):
    active_addon_name = StringProperty(name="Active Addon Name",description="Used to determine what the addon is active.")
    
    ui = PointerProperty(name="Interface",type= fd_interface)

    #These properties are used to view the children objects in a group
    active_object_name = StringProperty(name="Active Object Name",
                                        description="Used to make sure that the correct collection is being displayed this is set in the load child objects operator.")
    
    active_object_index = IntProperty(name="Active Object Index",
                                      description="Used is list views to select an object in the children_objects collection")
    
    children_objects = CollectionProperty(name="Children Objects",
                                          type=fd_item,
                                          description="Collection of all of the children objects of a group. Used to display in a list view.")

bpy.utils.register_class(fd_scene)

class fd_window_manager(PropertyGroup):
    wall_rotation = FloatProperty(name="Wall Rotation",description="Used to store the walls rotation")
    
bpy.utils.register_class(fd_window_manager)

class extend_blender_data():
    bpy.types.Object.mv = PointerProperty(type = fd_object)
    bpy.types.Scene.mv = PointerProperty(type = fd_scene)
    bpy.types.WindowManager.mv = PointerProperty(type = fd_window_manager)
    
class Assembly():
    
    obj_bp = None
    obj_x = None
    obj_y = None
    obj_z = None

    def __init__(self,obj_bp=None):
        if obj_bp:
            self.obj_bp = obj_bp
            for child in obj_bp.children:
                if child.mv.type == 'VPDIMX':
                    self.obj_x = child
                if child.mv.type == 'VPDIMY':
                    self.obj_y = child
                if child.mv.type == 'VPDIMZ':
                    self.obj_z = child
                if self.obj_bp and self.obj_x and self.obj_y and self.obj_z:
                    break

    def create_assembly(self):
        bpy.ops.object.select_all(action='DESELECT')
        verts = [(0, 0, 0)]
        mesh = bpy.data.meshes.new("Base Point")
        bm = bmesh.new()
        for v_co in verts:
            bm.verts.new(v_co)
        bm.to_mesh(mesh)
        mesh.update()
        obj_base = object_utils.object_data_add(bpy.context,mesh)
        obj_parent = obj_base.object
        obj_parent.location = (0,0,0)
        obj_parent.mv.type = 'BPASSEMBLY'
        obj_parent.mv.name_object = 'New Assembly'

        bpy.ops.object.empty_add()
        obj_x = bpy.context.active_object
        obj_x.name = "VPDIMX"
        obj_x.location = (0,0,0)
        obj_x.mv.type = 'VPDIMX'
        obj_x.lock_location[1] = True
        obj_x.lock_location[2] = True
        obj_x.parent = obj_parent

        bpy.ops.object.empty_add()
        obj_y = bpy.context.active_object
        obj_y.name = "VPDIMY"
        obj_y.location = (0,0,0)
        obj_y.mv.type = 'VPDIMY'
        obj_y.lock_location[0] = True
        obj_y.lock_location[2] = True
        obj_y.parent = obj_parent

        bpy.ops.object.empty_add()
        obj_z = bpy.context.active_object
        obj_z.name = "VPDIMZ"
        obj_z.location = (0,0,0)
        obj_z.mv.type = 'VPDIMZ'
        obj_z.lock_location[0] = True
        obj_z.lock_location[1] = True
        obj_z.parent = obj_parent
        
        self.obj_bp = obj_parent
        self.obj_x = obj_x
        self.obj_y = obj_y
        self.obj_z = obj_z
        
        obj_x.location.x = inches(10)
        obj_y.location.y = inches(10)
        obj_z.location.z = inches(10)
        
        self.set_object_names()
    
    def build_cage(self):
        if self.obj_bp and self.obj_x and self.obj_y and self.obj_z:
            size = (self.obj_x.location.x, self.obj_y.location.y, self.obj_z.location.z)
            obj_cage = create_cube_mesh('CAGE',size)
            obj_cage.mv.name_object = 'CAGE'
            obj_cage.location = (0,0,0)
            obj_cage.parent = self.obj_bp
            obj_cage.mv.type = 'CAGE'

            create_vertex_group_for_hooks(obj_cage,[2,3,6,7],'X Dimension')
            create_vertex_group_for_hooks(obj_cage,[1,2,5,6],'Y Dimension')
            create_vertex_group_for_hooks(obj_cage,[4,5,6,7],'Z Dimension')
            hook_vertex_group_to_object(obj_cage,'X Dimension',self.obj_x)
            hook_vertex_group_to_object(obj_cage,'Y Dimension',self.obj_y)
            hook_vertex_group_to_object(obj_cage,'Z Dimension',self.obj_z)
            
            obj_cage.draw_type = 'WIRE'
            obj_cage.hide_select = True
            obj_cage.lock_location = (True,True,True)
            obj_cage.lock_rotation = (True,True,True)
            obj_cage.cycles_visibility.camera = False
            obj_cage.cycles_visibility.diffuse = False
            obj_cage.cycles_visibility.glossy = False
            obj_cage.cycles_visibility.transmission = False
            obj_cage.cycles_visibility.shadow = False
            return obj_cage

    def get_cage(self):
        for child in self.obj_bp.children:
            if child.mv.type == 'CAGE':
                return child
        return self.build_cage()

    def get_var(self,data_path,var_name):
        if data_path == 'dim_x':
            return Variable(self.obj_x,'location.x',var_name)
        elif data_path == 'dim_y':
            return Variable(self.obj_y,'location.y',var_name)
        elif data_path == 'dim_z':
            return Variable(self.obj_z,'location.z',var_name)
        else:
            prompt_path = self.get_prompt_data_path(data_path)
            if prompt_path:
                return Variable(self.obj_bp,prompt_path,var_name)
            else:
                return Variable(self.obj_bp,data_path,var_name)
        
    def get_prompt_data_path(self,prompt_name):
        for prompt in self.obj_bp.mv.PromptPage.COL_Prompt:
            if prompt.name == prompt_name:
                if prompt.Type == 'NUMBER':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].NumberValue'
                if prompt.Type == 'QUANTITY':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].QuantityValue'
                if prompt.Type == 'COMBOBOX':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].EnumIndex'
                if prompt.Type == 'CHECKBOX':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].CheckBoxValue'
                if prompt.Type == 'TEXT':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].TextValue'
                if prompt.Type == 'DISTANCE':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].DistanceValue'
                if prompt.Type == 'ANGLE':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].AngleValue'
                if prompt.Type == 'PERCENTAGE':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].PercentageValue'
                if prompt.Type == 'PRICE':
                    return 'mv.PromptPage.COL_Prompt["' + prompt_name + '"].PriceValue'
        
    def delete_cage(self):
        list_obj_cage = []
        for child in self.obj_bp.children:
            if child.mv.type == 'CAGE':
                list_obj_cage.append(child)
                
        delete_obj_list(list_obj_cage)

    def replace(self,smart_group):
        copy_drivers(self.obj_bp,smart_group.obj_bp)
        copy_drivers(self.obj_x,smart_group.obj_x)
        copy_drivers(self.obj_y,smart_group.obj_y)
        copy_drivers(self.obj_z,smart_group.obj_z)
        obj_list = []
        obj_list.append(self.obj_bp)
        for child in self.obj_bp.children:
            obj_list.append(child)
        delete_obj_list(obj_list)

    def set_object_names(self):
        group_name = self.obj_bp.mv.name_object
        self.obj_bp.name = self.obj_bp.mv.type + "." + self.obj_bp.mv.name_object
        for child in self.obj_bp.children:
            if child.mv.type != 'NONE':
                if child.mv.name_object != "":
                    child.name = child.mv.type + "." + group_name + "." + child.mv.name_object
                else:
                    child.name = child.mv.type + "." + group_name
            else:
                if child.mv.name_object != "":
                    child.name = child.type + "." + group_name + "." + child.mv.name_object
                else:
                    child.name = child.type + "." + group_name

    def add_tab(self,name="",tab_type='VISIBLE',calc_type="XDIM"):
        tab = self.obj_bp.mv.PromptPage.COL_MainTab.add()
        tab.name = name
        tab.type = tab_type
        if tab_type == 'CALCULATOR':
            tab.calculator_type = calc_type

    def number_of_visible_prompt_tabs(self):
        number_of_tabs = 0
        for tab in self.obj_bp.mv.PromptPage.COL_MainTab:
            if tab.type == 'VISIBLE':
                number_of_tabs += 1
        return number_of_tabs

    def get_prompt(self,prompt_name):
        if prompt_name in self.obj_bp.mv.PromptPage.COL_Prompt:
            return self.obj_bp.mv.PromptPage.COL_Prompt[prompt_name]

    def add_prompt(self,name="",prompt_type='DISTANCE',value=False,lock=False,tab_index=0,items=[],columns=1,equal=False):
        prompt = self.obj_bp.mv.PromptPage.COL_Prompt.add()
        prompt.name = name
        prompt.Type = prompt_type
        prompt.lock_value = lock
        prompt.TabIndex = tab_index
        prompt.equal = equal #Only for calculators
        if prompt.Type == 'NUMBER':
            prompt.NumberValue = value
        if prompt.Type == 'QUANTITY':
            prompt.QuantityValue = value
        if prompt.Type == 'COMBOBOX':
            prompt.EnumIndex = value
            for combo_box_item in items:
                enum_item = prompt.COL_EnumItem.add()
                enum_item.name = combo_box_item
            prompt.columns =  columns
        if prompt.Type == 'CHECKBOX':
            prompt.CheckBoxValue = value
        if prompt.Type == 'TEXT':
            prompt.TextValue = value
        if prompt.Type == 'DISTANCE':
            prompt.DistanceValue = value
        if prompt.Type == 'ANGLE':
            prompt.AngleValue = value
        if prompt.Type == 'PERCENTAGE':
            prompt.PercentageValue = value
        if prompt.Type == 'PRICE':
            prompt.PriceValue = value

    def calc_width(self):
        """ Calculates the width of the group based on the rotation
            Used to determine collisions for rotated groups
        """
        return math.cos(self.obj_bp.rotation_euler.z) * self.obj_x.location.x
    
    def calc_depth(self):
        """ Calculates the depth of the group based on the rotation
            Used to determine collisions for rotated groups
        """
        #TODO: This not correct
        if self.obj_bp.rotation_euler.z < 0:
            return math.fabs(self.obj_x.location.x)
        else:
            return math.fabs(self.obj_y.location.y)
    
    def calc_x(self):
        """ Calculates the x location of the group based on the rotation
            Used to determine collisions for rotated groups
        """
        return math.sin(self.obj_bp.rotation_euler.z) * self.obj_y.location.y

    def refresh_hook_modifiers(self):
        for child in self.obj_bp.children:
            if child.type == 'MESH':
                bpy.ops.fd_object.apply_hook_modifiers(object_name=child.name)
                bpy.ops.fd_assembly.connect_meshes_to_hooks_in_assembly(object_name=child.name)

    def has_height_collision(self,group):
        """ Determines if this group will collide in the z with 
            the group that is passed in arg 2
        """
        if self.obj_bp.matrix_world[2][3] > self.obj_z.matrix_world[2][3]:
            grp1_z_1 = self.obj_z.matrix_world[2][3]
            grp1_z_2 = self.obj_bp.matrix_world[2][3]
        else:
            grp1_z_1 = self.obj_bp.matrix_world[2][3]
            grp1_z_2 = self.obj_z.matrix_world[2][3]
        
        if group.obj_bp.matrix_world[2][3] > group.obj_z.matrix_world[2][3]:
            grp2_z_1 = group.obj_z.matrix_world[2][3]
            grp2_z_2 = group.obj_bp.matrix_world[2][3]
        else:
            grp2_z_1 = group.obj_bp.matrix_world[2][3]
            grp2_z_2 = group.obj_z.matrix_world[2][3]
    
        if grp1_z_1 >= grp2_z_1 and grp1_z_1 <= grp2_z_2:
            return True
            
        if grp1_z_2 >= grp2_z_1 and grp1_z_2 <= grp2_z_2:
            return True
    
        if grp2_z_1 >= grp1_z_1 and grp2_z_1 <= grp1_z_2:
            return True
            
        if grp2_z_2 >= grp1_z_1 and grp2_z_2 <= grp1_z_2:
            return True

    def set_prompts(self,dict_prompts):
        for key in dict_prompts:
            if key in self.obj_bp.mv.PromptPage.COL_Prompt:
                prompt = self.obj_bp.mv.PromptPage.COL_Prompt[key]
                if prompt.Type == 'NUMBER':
                    prompt.NumberValue = dict_prompts[key]
                if prompt.Type == 'QUANTITY':
                    prompt.QuantityValue = dict_prompts[key]
                if prompt.Type == 'COMBOBOX':
                    prompt.EnumIndex = dict_prompts[key]
                if prompt.Type == 'CHECKBOX':
                    prompt.CheckBoxValue = dict_prompts[key]
                if prompt.Type == 'TEXT':
                    prompt.TextValue = dict_prompts[key]
                if prompt.Type == 'DISTANCE':
                    prompt.DistanceValue = dict_prompts[key]
                if prompt.Type == 'ANGLE':
                    prompt.AngleValue = dict_prompts[key]
                if prompt.Type == 'PERCENTAGE':
                    prompt.PercentageValue = dict_prompts[key]
                if prompt.Type == 'PRICE':
                    prompt.PriceValue = dict_prompts[key]

    def x_loc(self,expression,driver_vars):
        self.obj_bp.driver_add('location',0)
        for var in driver_vars:
            for DR in self.obj_bp.animation_data.drivers:
                if 'location' in DR.data_path and 0 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break

    def y_loc(self,expression,driver_vars):
        self.obj_bp.driver_add('location',1)
        for var in driver_vars:
            for DR in self.obj_bp.animation_data.drivers:
                if 'location' in DR.data_path and 1 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break

    def z_loc(self,expression,driver_vars):
        self.obj_bp.driver_add('location',2)
        for var in driver_vars:
            for DR in self.obj_bp.animation_data.drivers:
                if 'location' in DR.data_path and 2 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break

    def x_rot(self,expression,driver_vars):
        self.obj_bp.driver_add('rotation_euler',0)
        for var in driver_vars:
            for DR in self.obj_bp.animation_data.drivers:
                if 'rotation_euler' in DR.data_path and 0 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break

    def y_rot(self,expression,driver_vars):
        self.obj_bp.driver_add('rotation_euler',1)
        for var in driver_vars:
            for DR in self.obj_bp.animation_data.drivers:
                if 'rotation_euler' in DR.data_path and 1 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break
                
    def z_rot(self,expression,driver_vars):
        self.obj_bp.driver_add('rotation_euler',2)
        for var in driver_vars:
            for DR in self.obj_bp.animation_data.drivers:
                if 'rotation_euler' in DR.data_path and 2 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break

    def x_dim(self,expression,driver_vars):
        self.obj_x.driver_add('location',0)
        for var in driver_vars:
            for DR in self.obj_x.animation_data.drivers:
                if 'location' in DR.data_path and 0 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break

    def y_dim(self,expression,driver_vars):
        self.obj_y.driver_add('location',1)
        for var in driver_vars:
            for DR in self.obj_y.animation_data.drivers:
                if 'location' in DR.data_path and 1 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break
                          
    def z_dim(self,expression,driver_vars):
        self.obj_z.driver_add('location',2)
        for var in driver_vars:
            for DR in self.obj_z.animation_data.drivers:
                if 'location' in DR.data_path and 2 == DR.array_index:
                    new_var = DR.driver.variables.new()
                    new_var.name = var.var_name
                    new_var.targets[0].data_path = var.data_path
                    new_var.targets[0].id = var.obj
                    DR.driver.expression = expression
                    break
    
    def calculator_deduction(self,expression,driver_vars):
        for tab in self.obj_bp.mv.PromptPage.COL_MainTab:
            if tab.type == 'CALCULATOR':
                data_path = 'mv.PromptPage.COL_MainTab["' + tab.name + '"].calculator_deduction'
                self.obj_bp.driver_add(data_path)
                
                for var in driver_vars:
                    if self.obj_bp.animation_data:
                        for DR in self.obj_bp.animation_data.drivers:
                            if data_path in DR.data_path:
                                new_var = DR.driver.variables.new()
                                new_var.name = var.var_name
                                new_var.targets[0].data_path = var.data_path
                                new_var.targets[0].id = var.obj
                                DR.driver.expression = expression
                                break

    def prompt(self,prompt_name,expression,driver_vars):
        data_path = ""
        for prompt in self.obj_bp.mv.PromptPage.COL_Prompt:
            if prompt.name == prompt_name:
                if prompt.Type == 'NUMBER':
                    data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].NumberValue'
                    self.obj_bp.driver_add(data_path)
                if prompt.Type == 'QUANTITY':
                    data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].QuantityValue'
                    self.obj_bp.driver_add(data_path)
                if prompt.Type == 'COMBOBOX':
                    data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].EnumIndex'
                    self.obj_bp.driver_add(data_path)
                if prompt.Type == 'CHECKBOX':
                    data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].CheckBoxValue'
                    self.obj_bp.driver_add(data_path)
                if prompt.Type == 'DISTANCE':
                    data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].DistanceValue'
                    self.obj_bp.driver_add(data_path)
                if prompt.Type == 'ANGLE':
                    data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].AngleValue'
                    self.obj_bp.driver_add(data_path)
                if prompt.Type == 'PERCENTAGE':
                    data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].PercentageValue'
                    self.obj_bp.driver_add(data_path)
                if prompt.Type == 'PRICE':
                    data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].PriceValue'
                    self.obj_bp.driver_add(data_path)
                break
        if data_path != "":
            for var in driver_vars:
                if self.obj_bp.animation_data:
                    for DR in self.obj_bp.animation_data.drivers:
                        if data_path in DR.data_path:
                            new_var = DR.driver.variables.new()
                            new_var.name = var.var_name
                            new_var.targets[0].data_path = var.data_path
                            new_var.targets[0].id = var.obj
                            DR.driver.expression = expression
                            break
        else:
            print("Error: '" + prompt_name + "' not found while setting expression '" + expression + "'")

    def draw_properties(self,layout):
        ui = bpy.context.scene.mv.ui
        col = layout.column(align=True)
        box = col.box()
        row = box.row(align=True)
        row.prop_enum(ui, "group_tabs", enums.enum_group_tabs[0][0], icon='INFO', text="") 
        row.prop_enum(ui, "group_tabs", enums.enum_group_tabs[2][0], icon='SETTINGS', text="")    
        row.prop_enum(ui, "group_tabs", enums.enum_group_tabs[3][0], icon='GROUP', text="")
        row.prop_enum(ui, "group_tabs", enums.enum_group_tabs[4][0], icon='AUTO', text="")
        if ui.group_tabs == 'INFO':
            box = col.box()
            self.draw_transform(box)
        if ui.group_tabs == 'PROMPTS':
            box = col.box()
            self.obj_bp.mv.PromptPage.draw_prompt_page(box,self.obj_bp,allow_edit=True)
        if ui.group_tabs == 'OBJECTS':
            box = col.box()
            self.draw_objects(box)
        if ui.group_tabs == 'DRIVERS':
            box = col.box()
            self.draw_drivers(box)
    
    def draw_transform(self,layout,show_prompts=False):
        row = layout.row()
        row.operator('fd_assembly.rename_assembly',text=self.obj_bp.mv.name_object,icon='GROUP').object_name = self.obj_bp.name
        if show_prompts and self.number_of_visible_prompt_tabs() > 0:
            if self.number_of_visible_prompt_tabs() == 1:
                for index, tab in enumerate(self.obj_bp.mv.PromptPage.COL_MainTab):
                    if tab.type in {'VISIBLE','CALCULATOR'}:
                        props = row.operator("fd_prompts.show_object_prompts",icon='SETTINGS',text="")
                        props.object_bp_name = self.obj_bp.name
                        props.tab_name = tab.name
                        props.index = index
            else:
                row.menu('MENU_active_group_options',text="",icon='SETTINGS')

        row = layout.row(align=True)
        row.label('Dimension X:')
        row.prop(self.obj_x,"lock_location",index=0,text='')
        if self.obj_x.lock_location[0]:
            row.label(str(round(meter_to_unit(self.obj_x.location.x),4)))
        else:
            row.prop(self.obj_x,"location",index=0,text="")
            row.prop(self.obj_x,'hide',text="")
        
        row = layout.row(align=True)
        row.label('Dimension Y:')
        row.prop(self.obj_y,"lock_location",index=1,text='')
        if self.obj_y.lock_location[1]:
            row.label(str(round(meter_to_unit(self.obj_y.location.y),4)))
        else:
            row.prop(self.obj_y,"location",index=1,text="")
            row.prop(self.obj_y,'hide',text="")
        
        row = layout.row(align=True)
        row.label('Dimension Z:')
        row.prop(self.obj_z,"lock_location",index=2,text='')
        if self.obj_z.lock_location[2]:
            row.label(str(round(meter_to_unit(self.obj_z.location.z),4)))
        else:
            row.prop(self.obj_z,"location",index=2,text="")
            row.prop(self.obj_z,'hide',text="")
        
        col1 = layout.row()
        if self.obj_bp:
            col2 = col1.split()
            col = col2.column(align=True)
            col.label('Location:')
            
            if self.obj_bp.lock_location[0]:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_location",index=0,text="")
                row.label(str(round(meter_to_unit(self.obj_bp.location.x),4)))
            else:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_location",index=0,text="")
                row.prop(self.obj_bp,"location",index=0,text="X")
                
            if self.obj_bp.lock_location[1]:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_location",index=1,text="")
                row.label(str(round(meter_to_unit(self.obj_bp.location.y),4)))
            else:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_location",index=1,text="")
                row.prop(self.obj_bp,"location",index=1,text="Y")
                
            if self.obj_bp.lock_location[2]:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_location",index=2,text="")
                row.label(str(round(meter_to_unit(self.obj_bp.location.z),4)))
            else:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_location",index=2,text="")
                row.prop(self.obj_bp,"location",index=2,text="Z")
                
            col2 = col1.split()
            col = col2.column(align=True)
            col.label('Rotation:')
            
            if self.obj_bp.lock_rotation[0]:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_rotation",index=0,text="")
                row.label(str(round(math.degrees(self.obj_bp.rotation_euler.x),4)))
            else:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_rotation",index=0,text="")
                row.prop(self.obj_bp,"rotation_euler",index=0,text="X")
                
            if self.obj_bp.lock_rotation[1]:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_rotation",index=1,text="")
                row.label(str(round(math.degrees(self.obj_bp.rotation_euler.y),4)))
            else:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_rotation",index=1,text="")
                row.prop(self.obj_bp,"rotation_euler",index=1,text="Y")
                
            if self.obj_bp.lock_rotation[2]:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_rotation",index=2,text="")
                row.label(str(round(math.degrees(self.obj_bp.rotation_euler.z),4)))
            else:
                row = col.row(align=True)
                row.prop(self.obj_bp,"lock_rotation",index=2,text="")
                row.prop(self.obj_bp,"rotation_euler",index=2,text="Z")
                
    def draw_objects(self,layout):
        scene = bpy.context.scene
        row = layout.row()
        row.operator('fd_assembly.load_active_assembly_objects',text="Load Child Objects",icon='FILE_REFRESH').object_name = self.obj_bp.name
        row.menu('MENU_add_assembly_object',text="",icon='ZOOMIN')
        if self.obj_bp.name == scene.mv.active_object_name:
            col = layout.column(align=True)
            col.template_list("FD_UL_objects", " ", scene.mv, "children_objects", scene.mv, "active_object_index")
            if scene.mv.active_object_index <= len(scene.mv.children_objects) + 1:
                box = col.box()
                obj = bpy.data.objects[scene.mv.children_objects[scene.mv.active_object_index].name]
                box.prop(obj.mv,'name_object')
                if obj.type == 'MESH':
                    box.prop(obj.mv,'use_as_bool_obj')
                box.prop(obj.mv,'use_as_mesh_hook')
            
    def draw_drivers(self,layout):
        ui = bpy.context.scene.mv.ui
        row = layout.row(align=True)
        row.operator('fd_driver.turn_on_driver',text="",icon='QUIT').object_name = self.obj_bp.name
        row.prop(ui,'group_driver_tabs',text='')
        box = layout.box()
        
        if ui.group_driver_tabs == 'LOC_X':
            box.prop(self.obj_bp,'location',index=0,text="Location X")
            driver = get_driver(self.obj_bp,'location',0)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_bp.name,'location',0)
                draw_driver_variables(layout,driver,self.obj_bp.name)
    
        if ui.group_driver_tabs == 'LOC_Y':
            box.prop(self.obj_bp,'location',index=1,text="Location Y")
            driver = get_driver(self.obj_bp,'location',1)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_bp.name,'location',1)
                draw_driver_variables(layout,driver,self.obj_bp.name)
                
        if ui.group_driver_tabs == 'LOC_Z':
            box.prop(self.obj_bp,'location',index=2,text="Location Z")
            driver = get_driver(self.obj_bp,'location',2)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_bp.name,'location',2)
                draw_driver_variables(layout,driver,self.obj_bp.name)
                
        if ui.group_driver_tabs == 'ROT_X':
            box.prop(self.obj_bp,'rotation_euler',index=0,text="Rotation X")
            driver = get_driver(self.obj_bp,'rotation_euler',0)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_bp.name,'rotation_euler',0)
                draw_driver_variables(layout,driver,self.obj_bp.name)
                
        if ui.group_driver_tabs == 'ROT_Y':
            box.prop(self.obj_bp,'rotation_euler',index=1,text="Rotation Y")
            driver = get_driver(self.obj_bp,'rotation_euler',1)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_bp.name,'rotation_euler',1)
                draw_driver_variables(layout,driver,self.obj_bp.name)
    
        if ui.group_driver_tabs == 'ROT_Z':
            box.prop(self.obj_bp,'rotation_euler',index=2,text="Rotation Z")
            driver = get_driver(self.obj_bp,'rotation_euler',2)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_bp.name,'rotation_euler',2)
                draw_driver_variables(layout,driver,self.obj_bp.name)
    
        if ui.group_driver_tabs == 'DIM_X':
            box.prop(self.obj_x,'location',index=0,text="Dimension X")
            driver = get_driver(self.obj_x,'location',0)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_x.name,'location',0)
                draw_driver_variables(layout,driver,self.obj_x.name)
    
        if ui.group_driver_tabs == 'DIM_Y':
            box.prop(self.obj_y,'location',index=1,text="Dimension Y")
            driver = get_driver(self.obj_y,'location',1)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_y.name,'location',1)
                draw_driver_variables(layout,driver,self.obj_y.name)
                        
        if ui.group_driver_tabs == 'DIM_Z':
            box.prop(self.obj_z,'location',index=2,text="Dimension Z")
            driver = get_driver(self.obj_z,'location',2)
            if driver:
                draw_driver_expression(box,driver)
                draw_add_variable_operators(layout,self.obj_z.name,'location',2)
                draw_driver_variables(layout,driver,self.obj_z.name)
                
        if ui.group_driver_tabs == 'PROMPTS':
            if len(self.obj_bp.mv.PromptPage.COL_Prompt) < 1:
                box.label('No Prompts')
            else:
                box.template_list("FD_UL_promptitems"," ", self.obj_bp.mv.PromptPage, "COL_Prompt", self.obj_bp.mv.PromptPage, "PromptIndex",rows=5,type='DEFAULT')
                prompt = self.obj_bp.mv.PromptPage.COL_Prompt[self.obj_bp.mv.PromptPage.PromptIndex]
                
                if prompt.Type == 'DISTANCE':
                    driver = get_driver(self.obj_bp,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].DistanceValue',0)
                    if driver:
                        box.operator('fd_driver.turn_off_driver').object_name = self.obj_bp.name
                        draw_driver_expression(box,driver)
                        draw_add_variable_operators(layout,self.obj_bp.name,'mv.PromptPage.COL_Prompt["' +  prompt.name + '"].DistanceValue',0)
                        draw_driver_variables(layout,driver,self.obj_bp.name)
                        
                if prompt.Type == 'ANGLE':
                    driver = get_driver(self.obj_bp,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].AngleValue',0)
                    if driver:
                        box.operator('fd_driver.turn_off_driver').object_name = self.obj_bp.name
                        draw_driver_expression(box,driver)
                        draw_add_variable_operators(layout,self.obj_bp.name,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].AngleValue',0)
                        draw_driver_variables(layout,driver,self.obj_bp.name)
                        
                if prompt.Type == 'PRICE':
                    driver = get_driver(self.obj_bp,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].PriceValue',0)
                    if driver:
                        box.operator('fd_driver.turn_off_driver').object_name = self.obj_bp.name
                        draw_driver_expression(box,driver)
                        draw_add_variable_operators(layout,self.obj_bp.name,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].PriceValue',0)
                        draw_driver_variables(layout,driver,self.obj_bp.name)
                        
                if prompt.Type == 'PERCENTAGE':
                    driver = get_driver(self.obj_bp,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].PercentageValue',0)
                    if driver:
                        box.operator('fd_driver.turn_off_driver').object_name = self.obj_bp.name
                        draw_driver_expression(box,driver)
                        draw_add_variable_operators(layout,self.obj_bp.name,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].PercentageValue',0)
                        draw_driver_variables(layout,driver,self.obj_bp.name)
                
                if prompt.Type == 'NUMBER':
                    driver = get_driver(self.obj_bp,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].NumberValue',0)
                    if driver:
                        box.operator('fd_driver.turn_off_driver').object_name = self.obj_bp.name
                        draw_driver_expression(box,driver)
                        draw_add_variable_operators(layout,self.obj_bp.name,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].NumberValue',0)
                        draw_driver_variables(layout,driver,self.obj_bp.name)
                        
                if prompt.Type == 'QUANTITY':
                    driver = get_driver(self.obj_bp,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].QuantityValue',0)
                    if driver:
                        box.operator('fd_driver.turn_off_driver').object_name = self.obj_bp.name
                        draw_driver_expression(box,driver)
                        draw_add_variable_operators(layout,self.obj_bp.name,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].QuantityValue',0)
                        draw_driver_variables(layout,driver,self.obj_bp.name)
    
                if prompt.Type == 'COMBOBOX':
                    driver = get_driver(self.obj_bp,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].EnumIndex',0)
                    if driver:
                        box.operator('fd_driver.turn_off_driver').object_name = self.obj_bp.name
                        draw_driver_expression(box,driver)
                        draw_add_variable_operators(layout,self.obj_bp.name,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].EnumIndex',0)
                        draw_driver_variables(layout,driver,self.obj_bp.name)
    
                if prompt.Type == 'CHECKBOX':
                    driver = get_driver(self.obj_bp,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].CheckBoxValue',0)
                    if driver:
                        box.operator('fd_driver.turn_off_driver').object_name = self.obj_bp.name
                        draw_driver_expression(box,driver)
                        draw_add_variable_operators(layout,self.obj_bp.name,'mv.PromptPage.COL_Prompt["' + prompt.name + '"].CheckBoxValue',0)
                        draw_driver_variables(layout,driver,self.obj_bp.name)

class Wall(Assembly):
    
    def __init__(self,obj_bp=None):
        if obj_bp:
            self.obj_bp = obj_bp
            for child in obj_bp.children:
                if child.mv.type == 'VPDIMX':
                    self.obj_x = child
                if child.mv.type == 'VPDIMY':
                    self.obj_y = child
                if child.mv.type == 'VPDIMZ':
                    self.obj_z = child
                if self.obj_bp and self.obj_x and self.obj_y and self.obj_z:
                    break

    def create_wall(self):
        self.create_assembly()
        self.obj_bp.mv.type = 'BPWALL'
        self.obj_bp.mv.name_object = 'Wall'
        self.set_object_names()

    def build_wall_mesh(self):
        self.obj_bp.mv.name = "BPWALL.Wall"
        obj_mesh = self.build_cage()
        obj_mesh.mv.name = 'Wall Mesh'
        obj_mesh.name = "Wall Mesh"
        obj_mesh.mv.type = 'NONE'
        obj_mesh.draw_type = 'TEXTURED'
        obj_mesh.hide_select = False
        obj_mesh.cycles_visibility.camera = True
        obj_mesh.cycles_visibility.diffuse = True
        obj_mesh.cycles_visibility.glossy = True
        obj_mesh.cycles_visibility.transmission = True
        obj_mesh.cycles_visibility.shadow = True
        return obj_mesh
        
    def get_wall_mesh(self):
        for child in self.obj_bp.children:
            if child.type == 'MESH' and child.mv.type == 'NONE':
                return child
        
    def get_wall_groups(self):
        """ This returns a sorted list of all of the groups base points
            parented to the wall
        """
        list_obj_bp = []
        for child in self.obj_bp.children:
            if child.mv.type == 'BPASSEMBLY':
                list_obj_bp.append(child)
        list_obj_bp.sort(key=lambda obj: obj.location.x, reverse=False)
        return list_obj_bp
        
    def get_connected_wall(self,direction):
        if direction == 'LEFT':
            for con in self.obj_bp.constraints:
                if con.type == 'COPY_LOCATION':
                    if con.target:
                        return Wall(con.target.parent)
    
        if direction == 'RIGHT':
            for obj in bpy.context.visible_objects:
                if obj.mv.type == 'BPWALL':
                    next_wall = Wall(obj)
                    for con in next_wall.obj_bp.constraints:
                        if con.type == 'COPY_LOCATION':
                            if con.target == self.obj_x:
                                return next_wall
        
class Variable():
    
    var_name = ""
    obj = None
    data_path = ""
    
    def __init__(self,obj,data_path,var_name):
        self.obj = obj
        self.data_path = data_path
        self.var_name = var_name
    
#-------OBJECT CREATION

def create_cube_mesh(name,size):
    
    verts = [(0.0, 0.0, 0.0),
             (0.0, size[1], 0.0),
             (size[0], size[1], 0.0),
             (size[0], 0.0, 0.0),
             (0.0, 0.0, size[2]),
             (0.0, size[1], size[2]),
             (size[0], size[1], size[2]),
             (size[0], 0.0, size[2]),
             ]

    faces = [(0, 1, 2, 3),
             (4, 7, 6, 5),
             (0, 4, 5, 1),
             (1, 5, 6, 2),
             (2, 6, 7, 3),
             (4, 0, 3, 7),
            ]
    
    return create_object_from_verts_and_faces(verts,faces,name)

def create_floor_mesh(name,size):
    
    verts = [(0.0, 0.0, 0.0),
             (0.0, size[1], 0.0),
             (size[0], size[1], 0.0),
             (size[0], 0.0, 0.0),
            ]

    faces = [(0, 1, 2, 3),
            ]

    return create_object_from_verts_and_faces(verts,faces,name)

def create_object_from_verts_and_faces(verts,faces,name):
    """ Creates an object from Verties and Faces
        arg1: Verts List of tuples [(float,float,float)]
        arg2: Faces List of ints
    """
    mesh = bpy.data.meshes.new(name)
    
    bm = bmesh.new()

    for v_co in verts:
        bm.verts.new(v_co)
    
    for f_idx in faces:
        bm.verts.ensure_lookup_table()
        bm.faces.new([bm.verts[i] for i in f_idx])
    
    bm.to_mesh(mesh)
    
    mesh.update()
    
    obj_new = bpy.data.objects.new(mesh.name, mesh)
    
    bpy.context.scene.objects.link(obj_new)
    return obj_new
    
#-------OBJECT MODIFICATIONS
def apply_hook_modifiers(obj):
    obj.hide = False
    obj.select = True
    bpy.context.scene.objects.active = obj
    for mod in obj.modifiers:
        if mod.type == 'HOOK':
            bpy.ops.object.modifier_apply(modifier=mod.name)

def connect_objects_location(anchor_obj,obj):
    constraint = obj.constraints.new('COPY_LOCATION')
    constraint.target = anchor_obj
    constraint.use_x = True
    constraint.use_y = True
    constraint.use_z = True

def create_vertex_group_for_hooks(obj_mesh,vert_list,vgroupname):
    """ Adds a new vertex group to a mesh. If the group already exists
        Then no group is added. The second parameter allows you to assign
        verts to the vertex group.
        Arg1: bpy.data.Object | Mesh Object to operate on
        Arg2: [] of int | vertext indexs to assign to group
        Arg3: string | vertex group name
    """
    if vgroupname not in obj_mesh.vertex_groups:
        obj_mesh.vertex_groups.new(name=vgroupname)
        
    vgroup = obj_mesh.vertex_groups[vgroupname]
    vgroup.add(vert_list,1,'ADD')

def delete_obj_list(obj_list):
    #USED IN CABINET LIBRARY
    bpy.ops.object.select_all(action='DESELECT')
    
    for obj in obj_list:
        if obj.animation_data:
            for driver in obj.animation_data.drivers:
                if driver.data_path in {'hide','hide_select'}: # THESE DRIVERS MUST BE REMOVED TO DELETE OBJECTS
                    obj.driver_remove(driver.data_path) 
        
        obj.parent = None
        obj.hide_select = False
        obj.hide = False
        obj.select = True

#     bpy.ops.group.objects_remove_all()
    bpy.ops.object.delete(use_global=True)
    
#     TODO: I HAVE HAD PROBLEMS WITH THIS CRASHING BLENDER    
#           FIGURE OUT HOW TO REMOVE DATA FROM THE BLEND FILE.
#     for obj in obj_list:
#         obj.user_clear()
#         bpy.data.objects.remove(obj)
        
def delete_assembly_from_bp(obj_bp):
    """ Deletes the group this is used in the cabinet library
    """
    obj_list = []
    obj_list.append(obj_bp)
    for child in obj_bp.children:
        if len(child.children) > 0:
            delete_assembly_from_bp(child)
        else:
            obj_list.append(child)
    delete_obj_list(obj_list)

def hook_vertex_group_to_object(obj_mesh,vertex_group,obj_hook):
    bpy.ops.object.select_all(action = 'DESELECT')
    obj_hook.hide = False
    obj_hook.hide_select = False
    obj_hook.select = True
    obj_mesh.hide = False
    obj_mesh.hide_select = False
    if vertex_group in obj_mesh.vertex_groups:
        vgroup = obj_mesh.vertex_groups[vertex_group]
        obj_mesh.vertex_groups.active_index = vgroup.index
        bpy.context.scene.objects.active = obj_mesh
        bpy.ops.fd_object.toggle_edit_mode(object_name=obj_mesh.name)
        bpy.ops.mesh.select_all(action = 'DESELECT')
        bpy.ops.object.vertex_group_select()
        if obj_mesh.data.total_vert_sel > 0:
            bpy.ops.object.hook_add_selob()
        bpy.ops.mesh.select_all(action = 'DESELECT')
        bpy.ops.fd_object.toggle_edit_mode(object_name=obj_mesh.name)
        
def link_objects_to_scene(obj_bp,scene):
    # USED IN CABINET LIBRARY
    scene.objects.link(obj_bp)
    #obj_bp.draw_type = 'WIRE'
    for child in obj_bp.children:
        child.draw_type = 'WIRE' #THIS IS USED FOR THE NEW DRAG AND DROP
        child.select = False
        if child.parent is not None:
            child.parent = child.parent #THIS IS NEEDED TO ALLOW MODAL PLACEMENT 
        if len(child.children) > 0:
            link_objects_to_scene(child,scene)
        else:
            scene.objects.link(child)
            if child.type == 'EMPTY':
                child.hide = True
        
def update_vector_groups(obj_bp):
    vgroupslist = []
    vgroupslist.append('X Dimension') #THIS IS USED FOR ALL MESHES
    vgroupslist.append('Y Dimension') #THIS IS USED FOR ALL MESHES
    vgroupslist.append('Z Dimension') #THIS IS USED FOR ALL MESHES
    objlist = []
    
    for child in obj_bp.children:
        if child.type == 'EMPTY' and child.mv.use_as_mesh_hook:
            vgroupslist.append(child.mv.name_object)
        if child.type == 'MESH' and child.mv.type not in {'BPASSEMBLY','BPWALL'}:
            objlist.append(child)
    
    for obj in objlist:
        for vgroup in vgroupslist:
            if vgroup not in obj.vertex_groups:
                obj.vertex_groups.new(name=vgroup)
        
def remove_referenced_modifiers(obj_ref):
    """ This is removes modifers that use this object
        At this time this only removes boolean modifiers for walls.
        This is called when a wall or door is deleted
    """
    for obj in bpy.context.visible_objects:
        if obj.mv.type == 'NONE' and obj.type == 'MESH':
            for mod in obj.modifiers:
                if mod.type == 'BOOLEAN':
                    if mod.object == obj_ref:
                        obj.modifiers.remove(mod)
    
def copy_drivers(obj,obj_target):
    if obj.animation_data:
        for driver in obj.animation_data.drivers:
            if 'mv.PromptPage' not in driver.data_path:
                newdriver = None
                try:
                    newdriver = obj_target.driver_add(driver.data_path,driver.array_index)
                except Exception:
                    try:
                        newdriver = obj_target.driver_add(driver.data_path)
                    except Exception:
                        print("Unable to Copy Prompt Driver", driver.data_path)
                if newdriver:
                    newdriver.driver.expression = driver.driver.expression
                    newdriver.driver.type = driver.driver.type
                    for var in driver.driver.variables:
                        if var.name not in newdriver.driver.variables:
                            newvar = newdriver.driver.variables.new()
                            newvar.name = var.name
                            newvar.type = var.type
                            for index, target in enumerate(var.targets):
                                newtarget = newvar.targets[index]
                                if target.id is obj:
                                    newtarget.id = obj_target #CHECK SELF REFERENCE FOR PROMPTS
                                else:
                                    newtarget.id = target.id
                                newtarget.transform_space = target.transform_space
                                newtarget.transform_type = target.transform_type
                                newtarget.data_path = target.data_path

def has_hook_modifier(obj):
    for mod in obj.modifiers:
        if mod.type == 'HOOK':
            return True
    return False

#-------DATA GETTERS
def get_version_string():
    version = bpy.app.version
    return str(version[0]) + "." + str(version[1])

def get_wall_bp(obj):
    """ This will get the wall base point from the passed in object
    """
    if obj:
        if obj.mv.type == 'BPWALL':
            return obj
        else:
            if obj.parent:
                return get_wall_bp(obj.parent)
            
def get_parent_assembly_bp(obj):
    """ This will get the top level group base point from the passed in object
    """
    if obj:
        if obj.parent:
            if obj.parent.mv.type == 'BPASSEMBLY':
                return get_parent_assembly_bp(obj.parent)
            else:
                if obj.parent.mv.type == 'BPWALL' and obj.mv.type == 'BPASSEMBLY':
                    return obj
        else:
            if obj.mv.type == 'BPASSEMBLY':
                return obj
            
def get_assembly_bp(obj):
    """ This will get the group base point from the passed in object
    """
    if obj:
        if obj.mv.type == 'BPASSEMBLY':
            return obj
        else:
            if obj.parent:
                return get_assembly_bp(obj.parent)

#TODO: THIS SHOULD BE MOVED TO THE GROUP CLASS
def get_collision_location(obj_product_bp,direction='LEFT'):
    group = Assembly(obj_product_bp)
    if group.obj_bp.parent:
        wall = Wall(group.obj_bp.parent)
    list_obj_bp = wall.get_wall_groups()
    list_obj_left_bp = []
    list_obj_right_bp = []
    for index, obj_bp in enumerate(list_obj_bp):
        if obj_bp.name == group.obj_bp.name:
            list_obj_left_bp = list_obj_bp[:index]
            list_obj_right_bp = list_obj_bp[index + 1:]
            break
    if direction == 'LEFT':
        list_obj_left_bp.reverse()
        for obj_bp in list_obj_left_bp:
            prev_group = Assembly(obj_bp)
            if group.has_height_collision(prev_group):
                return obj_bp.location.x + prev_group.calc_width()
        
        # CHECK NEXT WALL
#         if math.radians(wall.obj_bp.rotation_euler.z) < 0:
        left_wall =  wall.get_connected_wall('LEFT')
        if left_wall:
            rotation_difference = wall.obj_bp.rotation_euler.z - left_wall.obj_bp.rotation_euler.z
            if rotation_difference < 0:
                list_obj_bp = left_wall.get_wall_groups()
                for obj in list_obj_bp:
                    prev_group = Assembly(obj)
                    product_x = obj.location.x
                    product_width = prev_group.calc_width()
                    x_dist = left_wall.obj_x.location.x  - (product_x + product_width)
                    product_depth = math.fabs(group.obj_y.location.y)
                    if x_dist <= product_depth:
                        if group.has_height_collision(prev_group):
                            return prev_group.calc_depth()
        return 0
    
    if direction == 'RIGHT':
        for obj_bp in list_obj_right_bp:
            next_group = Assembly(obj_bp)
            if group.has_height_collision(next_group):
                return obj_bp.location.x - next_group.calc_x()

        # CHECK NEXT WALL
        right_wall =  wall.get_connected_wall('RIGHT')
        if right_wall:
            rotation_difference = wall.obj_bp.rotation_euler.z - right_wall.obj_bp.rotation_euler.z
            if rotation_difference > 0:
                list_obj_bp = right_wall.get_wall_groups()
                for obj in list_obj_bp:
                    next_group = Assembly(obj)
                    product_x = obj.location.x
                    product_width = next_group.calc_width()
                    product_depth = math.fabs(group.obj_y.location.y)
                    if product_x <= product_depth:
                        if group.has_height_collision(next_group):
                            wall_length = wall.obj_x.location.x
                            product_depth = next_group.calc_depth()
                            return wall_length - product_depth

        return wall.obj_x.location.x

def get_driver(obj,data_path,array_index):
    if obj.animation_data:
        for DR in obj.animation_data.drivers:
            if DR.data_path == data_path and DR.array_index == array_index:
                return DR

def get_child_objects(obj,obj_list=None):
    #USED IN CABINET LIBRARY
    if not obj_list:
        obj_list = []
    if obj not in obj_list:
        obj_list.append(obj)
    for child in obj.children:
        obj_list.append(child)
        get_child_objects(child,obj_list)
    return obj_list

def get_selected_file_from_file_browser(context):
    #THIS IS USED BY THE CABINET LIBRARY
    window = context.window
    for area in window.screen.areas:
        if area.type == 'FILE_BROWSER':
            for space in area.spaces:
                if space.type == 'FILE_BROWSER':
                    return os.path.join(space.params.directory,space.params.filename)
                
def get_file_browser_path(context):
    for area in context.screen.areas:
        if area.type == 'FILE_BROWSER':
            for space in area.spaces:
                if space.type == 'FILE_BROWSER':
                    params = space.params
                    return params.directory
                
#-------MATH & GEO FUNCTIONS
        
def calc_distance(point1,point2):
    """ This gets the distance between two points (X,Y,Z)
    """
    return math.sqrt((point1[0]-point2[0])**2 + (point1[1]-point2[1])**2 + (point1[2]-point2[2])**2) 

#-------MODAL CALLBACKS
    
def get_selection_point(context, event, ray_max=10000.0,objects=None):
    """Gets the point to place an object based on selection"""
    # get the context arguments
    scene = context.scene
    region = context.region
    rv3d = context.region_data
    coord = event.mouse_region_x, event.mouse_region_y

    # get the ray from the viewport and mouse
    view_vector = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
    ray_origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
    ray_target = ray_origin + (view_vector * ray_max)

    def visible_objects_and_duplis():
        """Loop over (object, matrix) pairs (mesh only)"""

        for obj in context.visible_objects:
            
            if objects:
                if obj in objects:
                    yield (obj, obj.matrix_world.copy())
            
            else:
                if obj.draw_type != 'WIRE':
                    if obj.type == 'MESH':
                        if obj.mv.type not in {'BPASSEMBLY','BPWALL'}:
                            yield (obj, obj.matrix_world.copy())
        
                    if obj.dupli_type != 'NONE':
                        obj.dupli_list_create(scene)
                        for dob in obj.dupli_list:
                            obj_dupli = dob.object
                            if obj_dupli.type == 'MESH':
                                yield (obj_dupli, dob.matrix.copy())

            obj.dupli_list_clear()

    def obj_ray_cast(obj, matrix):
        """Wrapper for ray casting that moves the ray into object space"""

        # get the ray relative to the object
        matrix_inv = matrix.inverted()
        ray_origin_obj = matrix_inv * ray_origin
        ray_target_obj = matrix_inv * ray_target

        # cast the ray
        hit, normal, face_index = obj.ray_cast(ray_origin_obj, ray_target_obj)

        if face_index != -1:
            return hit, normal, face_index
        else:
            return None, None, None

    best_length_squared = ray_max * ray_max
    best_obj = None
    best_hit = scene.cursor_location
    for obj, matrix in visible_objects_and_duplis():
        if obj.type == 'MESH':
            if obj.data:
                hit, normal, face_index = obj_ray_cast(obj, matrix)
                if hit is not None:
                    hit_world = matrix * hit
                    length_squared = (hit_world - ray_origin).length_squared
                    if length_squared < best_length_squared:
                        best_hit = hit_world
                        best_length_squared = length_squared
                        best_obj = obj
                        
    return best_hit, best_obj
    
# def draw_callback_px(self, context):
#     font_id = 0  # XXX, need to find out how best to get this.
# 
#     offset = 10
#     text_height = 10
#     text_length = int(len(self.mouse_text) * 7.3)
#     
#     # draw some text
#     if self.center_header_text:
#         blf.position(font_id, context.area.width/2, context.area.height/2, 0)
#     else:
#         blf.position(font_id, 20, context.area.height - 105, 0)
#     blf.size(font_id, 20, 72)
#     blf.draw(font_id, self.header_text)
# 
#     if self.mouse_text != "":
#         # 50% alpha, 2 pixel width line
#         bgl.glEnable(bgl.GL_BLEND)
#         bgl.glColor4f(0.0, 0.0, 0.0, 0.5)
#         bgl.glLineWidth(10)
#     
#         bgl.glBegin(bgl.GL_LINE_STRIP)
#         bgl.glVertex2i(self.mouse_loc[0]-offset-5, self.mouse_loc[1]+offset)
#         bgl.glVertex2i(self.mouse_loc[0]+text_length-offset, self.mouse_loc[1]+offset)
#         bgl.glVertex2i(self.mouse_loc[0]+text_length-offset, self.mouse_loc[1]+offset+text_height)
#         bgl.glVertex2i(self.mouse_loc[0]-offset-5, self.mouse_loc[1]+offset+text_height)
#         bgl.glEnd()
#     
#         bgl.glColor4f(1.0, 1.0, 1.0, 1.0)
#         blf.position(font_id, self.mouse_loc[0]-offset, self.mouse_loc[1]+offset, 0)
#         blf.size(font_id, 15, 72)
#         blf.draw(font_id, self.mouse_text)
# 
#     # restore opengl defaults
#     bgl.glLineWidth(1)
#     bgl.glDisable(bgl.GL_BLEND)
#     bgl.glColor4f(0.0, 0.0, 0.0, 1.0)
    
def draw_callback_px(self, context):
    font_id = 0  # XXX, need to find out how best to get this.

    offset = 10
    text_height = 10
    text_length = int(len(self.mouse_text) * 7.3)
    
    if self.header_text != "":
        blf.size(font_id, 17, 72)
        text_w, text_h = blf.dimensions(font_id,self.header_text)
        blf.position(font_id, context.area.width/2 - text_w/2, context.area.height - 50, 0)
        blf.draw(font_id, self.header_text)

    # 50% alpha, 2 pixel width line
    if self.mouse_text != "":
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glColor4f(0.0, 0.0, 0.0, 0.5)
        bgl.glLineWidth(10)
    
        bgl.glBegin(bgl.GL_LINE_STRIP)
        bgl.glVertex2i(self.mouse_loc[0]-offset-5, self.mouse_loc[1]+offset)
        bgl.glVertex2i(self.mouse_loc[0]+text_length-offset, self.mouse_loc[1]+offset)
        bgl.glVertex2i(self.mouse_loc[0]+text_length-offset, self.mouse_loc[1]+offset+text_height)
        bgl.glVertex2i(self.mouse_loc[0]-offset-5, self.mouse_loc[1]+offset+text_height)
        bgl.glEnd()
        
        bgl.glColor4f(1.0, 1.0, 1.0, 1.0)
        blf.position(font_id, self.mouse_loc[0]-offset, self.mouse_loc[1]+offset, 0)
        blf.size(font_id, 15, 72)
        blf.draw(font_id, self.mouse_text)
        
    # restore opengl defaults
    bgl.glLineWidth(1)
    bgl.glDisable(bgl.GL_BLEND)
    bgl.glColor4f(0.0, 0.0, 0.0, 1.0)
    
#-------XML
    
def format_xml_file(path):
    """ This makes the xml file readable as a txt doc.
        For some reason the xml.toprettyxml() function
        adds extra blank lines. This makes the xml file
        unreadable. This function just removes
        # all of the blank lines.
        arg1: path to xml file
    """
    from xml.dom.minidom import parse
    
    xml = parse(path)
    pretty_xml = xml.toprettyxml()
    
    file = open(path,'w')
    file.write(pretty_xml)
    file.close()
    
    cleaned_lines = []
    with open(path,"r") as f:
        lines = f.readlines()
        for l in lines:
            l.strip()
            if "<" in l:
                cleaned_lines.append(l)
        
    with open (path,"w") as f:
        f.writelines(cleaned_lines)
    
#-------UNIT CONVERSION

def unit(number):
    # THIS IS USED BY THE CABINET LIBRARY FOR EXPORTING XML DATA
    if bpy.context.scene.unit_settings.system == 'METRIC':
        number = number * 1000 #METER TO MILLIMETER
    else:
        number = meter_to_unit(number) #METER TO INCH
    return number

def meter_to_unit(number):
    """ converts meter to current unit
    """
    if bpy.context.scene.unit_settings.system == 'METRIC':
        return number * 100
    else:
        return round(number * 39.3700787,4)

def inches(number):
    """ Converts Inch to meter
    """
    return round(number / 39.3700787,6)

def millimeters(number):
    """ Converts Millimeter to meter
    """
    return round(number * .001,2)

#-------INTERFACES

def update_file_browser_space(context,path):
    for area in context.screen.areas:
        if area.type == 'FILE_BROWSER':
            for space in area.spaces:
                if space.type == 'FILE_BROWSER':
                    params = space.params
                    params.directory = path
                    if not context.screen.show_fullscreen:
                        params.use_filter = True
                        params.display_type = 'THUMBNAIL'
                        params.use_filter_movie = False
                        params.use_filter_script = False
                        params.use_filter_sound = False
                        params.use_filter_text = False
                        params.use_filter_font = False
                        params.use_filter_folder = False
                        params.use_filter_blender = False
                        params.use_filter_image = True
    bpy.ops.file.next() #REFRESH FILEBROWSER INTERFACE

def draw_modifier(mod,layout,obj):
    
    def draw_show_expanded(mod,layout):
        if mod.show_expanded:
            layout.prop(mod,'show_expanded',text="",emboss=False)
        else:
            layout.prop(mod,'show_expanded',text="",emboss=False)
    
    def draw_apply_close(layout,mod_name):
        layout.operator('object.modifier_apply',text="",icon='EDIT_VEC',emboss=False).modifier = mod.name
        layout.operator('object.modifier_remove',text="",icon='PANEL_CLOSE',emboss=False).modifier = mod.name
    
    def draw_array_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_ARRAY')
        draw_apply_close(row,mod.name)
        
        if mod.show_expanded:
            box = col.box()
            box.prop(mod, "fit_type")
    
            if mod.fit_type == 'FIXED_COUNT':
                box.prop(mod, "count")
            elif mod.fit_type == 'FIT_LENGTH':
                box.prop(mod, "fit_length")
            elif mod.fit_type == 'FIT_CURVE':
                box.prop(mod, "curve")
    
            box.separator()
    
            split = box.split()
    
            col = split.column()
            col.prop(mod, "use_constant_offset")
            sub = col.column()
            sub.active = mod.use_constant_offset
            sub.prop(mod, "constant_offset_displace", text="")
    
            col.separator()
    
            col.prop(mod, "use_merge_vertices", text="Merge")
            sub = col.column()
            sub.active = mod.use_merge_vertices
            sub.prop(mod, "use_merge_vertices_cap", text="First Last")
            sub.prop(mod, "merge_threshold", text="Distance")
    
            col = split.column()
            col.prop(mod, "use_relative_offset")
            sub = col.column()
            sub.active = mod.use_relative_offset
            sub.prop(mod, "relative_offset_displace", text="")
    
            col.separator()
    
            col.prop(mod, "use_object_offset")
            sub = col.column()
            sub.active = mod.use_object_offset
            sub.prop(mod, "offset_object", text="")
    
            box.separator()
    
            box.prop(mod, "start_cap")
            box.prop(mod, "end_cap")
            
    def draw_bevel_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_BEVEL')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            split = box.split()
    
            col = split.column()
            col.prop(mod, "width")
            col.prop(mod, "segments")
            col.prop(mod, "profile")
    
            col = split.column()
            col.prop(mod, "use_only_vertices")
            col.prop(mod, "use_clamp_overlap")
    
            box.label(text="Limit Method:")
            box.row().prop(mod, "limit_method", expand=True)
            if mod.limit_method == 'ANGLE':
                box.prop(mod, "angle_limit")
            elif mod.limit_method == 'VGROUP':
                box.label(text="Vertex Group:")
                box.prop_search(mod, "vertex_group", obj, "vertex_groups", text="")
    
            box.label(text="Width Method:")
            box.row().prop(mod, "offset_type", expand=True)
    
    def draw_boolean_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_BOOLEAN')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            split = box.split()
    
            col = split.column()
            col.label(text="Operation:")
            col.prop(mod, "operation", text="")
    
            col = split.column()
            col.label(text="Object:")
            col.prop(mod, "object", text="")
    
    def draw_curve_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_CURVE')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            split = box.split()
    
            col = split.column()
            col.label(text="Object:")
            col.prop(mod, "object", text="")
            col = split.column()
            col.label(text="Vertex Group:")
            col.prop_search(mod, "vertex_group", obj, "vertex_groups", text="")
            box.label(text="Deformation Axis:")
            box.row().prop(mod, "deform_axis", expand=True)
    
    def draw_decimate_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_DECIM')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            decimate_type = mod.decimate_type
    
            row = box.row()
            row.prop(mod, "decimate_type", expand=True)
    
            if decimate_type == 'COLLAPSE':
                box.prop(mod, "ratio")
    
                split = box.split()
                row = split.row(align=True)
                row.prop_search(mod, "vertex_group", obj, "vertex_groups", text="")
                row.prop(mod, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')
    
                split.prop(mod, "use_collapse_triangulate")
            elif decimate_type == 'UNSUBDIV':
                box.prop(mod, "iterations")
            else:  # decimate_type == 'DISSOLVE':
                box.prop(mod, "angle_limit")
                box.prop(mod, "use_dissolve_boundaries")
                box.label("Delimit:")
                row = box.row()
                row.prop(mod, "delimit")
    
            box.label(text=iface_("Face Count: %d") % mod.face_count, translate=False)
    
    def draw_edge_split_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_EDGESPLIT')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            split = box.split()
    
            col = split.column()
            col.prop(mod, "use_edge_angle", text="Edge Angle")
            sub = col.column()
            sub.active = mod.use_edge_angle
            sub.prop(mod, "split_angle")
    
            split.prop(mod, "use_edge_sharp", text="Sharp Edges")
    
    def draw_hook_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='HOOK')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            split = box.split()
    
            col = split.column()
            col.label(text="Object:")
            col.prop(mod, "object", text="")
            if mod.object and mod.object.type == 'ARMATURE':
                col.label(text="Bone:")
                col.prop_search(mod, "subtarget", mod.object.data, "bones", text="")
            col = split.column()
            col.label(text="Vertex Group:")
            col.prop_search(mod, "vertex_group", obj, "vertex_groups", text="")
    
            layout.separator()
    
            split = box.split()
    
            col = split.column()
            col.prop(mod, "falloff")
            col.prop(mod, "force", slider=True)
    
            col = split.column()
            col.operator("object.hook_reset", text="Reset")
            col.operator("object.hook_recenter", text="Recenter")
    
            if obj.mode == 'EDIT':
                layout.separator()
                row = layout.row()
                row.operator("object.hook_select", text="Select")
                row.operator("object.hook_assign", text="Assign")
    
    def draw_mask_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_MASK')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            split = box.split()
    
            col = split.column()
            col.label(text="Mode:")
            col.prop(mod, "mode", text="")
    
            col = split.column()
            if mod.mode == 'ARMATURE':
                col.label(text="Armature:")
                col.prop(mod, "armature", text="")
            elif mod.mode == 'VERTEX_GROUP':
                col.label(text="Vertex Group:")
                row = col.row(align=True)
                row.prop_search(mod, "vertex_group", obj, "vertex_groups", text="")
                sub = row.row(align=True)
                sub.active = bool(mod.vertex_group)
                sub.prop(mod, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')
    
    def draw_mirror_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_MIRROR')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            split = box.split(percentage=0.25)
    
            col = split.column()
            col.label(text="Axis:")
            col.prop(mod, "use_x")
            col.prop(mod, "use_y")
            col.prop(mod, "use_z")
    
            col = split.column()
            col.label(text="Options:")
            col.prop(mod, "use_mirror_merge", text="Merge")
            col.prop(mod, "use_clip", text="Clipping")
            col.prop(mod, "use_mirror_vertex_groups", text="Vertex Groups")
    
            col = split.column()
            col.label(text="Textures:")
            col.prop(mod, "use_mirror_u", text="U")
            col.prop(mod, "use_mirror_v", text="V")
    
            col = box.column()
    
            if mod.use_mirror_merge is True:
                col.prop(mod, "merge_threshold")
            col.label(text="Mirror Object:")
            col.prop(mod, "mirror_object", text="") 
    
    def draw_solidify_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_SOLIDIFY')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            split = box.split()
    
            col = split.column()
            col.prop(mod, "thickness")
            col.prop(mod, "thickness_clamp")
    
            col.separator()
    
            row = col.row(align=True)
            row.prop_search(mod, "vertex_group", obj, "vertex_groups", text="")
            sub = row.row(align=True)
            sub.active = bool(mod.vertex_group)
            sub.prop(mod, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')
    
            sub = col.row()
            sub.active = bool(mod.vertex_group)
            sub.prop(mod, "thickness_vertex_group", text="Factor")
    
            col.label(text="Crease:")
            col.prop(mod, "edge_crease_inner", text="Inner")
            col.prop(mod, "edge_crease_outer", text="Outer")
            col.prop(mod, "edge_crease_rim", text="Rim")
    
            col = split.column()
    
            col.prop(mod, "offset")
            col.prop(mod, "use_flip_normals")
    
            col.prop(mod, "use_even_offset")
            col.prop(mod, "use_quality_normals")
            col.prop(mod, "use_rim")
    
            col.separator()
    
            col.label(text="Material Index Offset:")
    
            sub = col.column()
            row = sub.split(align=True, percentage=0.4)
            row.prop(mod, "material_offset", text="")
            row = row.row(align=True)
            row.active = mod.use_rim
            row.prop(mod, "material_offset_rim", text="Rim")
    
    def draw_subsurf_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_SUBSURF')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            box.row().prop(mod, "subdivision_type", expand=True)
    
            split = box.split()
            col = split.column()
            col.label(text="Subdivisions:")
            col.prop(mod, "levels", text="View")
            col.prop(mod, "render_levels", text="Render")
    
            col = split.column()
            col.label(text="Options:")
            col.prop(mod, "use_subsurf_uv")
            col.prop(mod, "show_only_control_edges")
    
    def draw_skin_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_SKIN')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            box.operator("object.skin_armature_create", text="Create Armature")
    
            box.separator()
    
            col = box.column(align=True)
            col.prop(mod, "branch_smoothing")
            col.prop(mod, "use_smooth_shade")
    
            split = box.split()
    
            col = split.column()
            col.label(text="Selected Vertices:")
            sub = col.column(align=True)
            sub.operator("object.skin_loose_mark_clear", text="Mark Loose").action = 'MARK'
            sub.operator("object.skin_loose_mark_clear", text="Clear Loose").action = 'CLEAR'
    
            sub = col.column()
            sub.operator("object.skin_root_mark", text="Mark Root")
            sub.operator("object.skin_radii_equalize", text="Equalize Radii")
    
            col = split.column()
            col.label(text="Symmetry Axes:")
            col.prop(mod, "use_x_symmetry")
            col.prop(mod, "use_y_symmetry")
            col.prop(mod, "use_z_symmetry")
    
    def draw_triangulate_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_TRIANGULATE')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            row = box.row()
    
            col = row.column()
            col.label(text="Quad Method:")
            col.prop(mod, "quad_method", text="")
            col = row.column()
            col.label(text="Ngon Method:")
            col.prop(mod, "ngon_method", text="")  
    
    def draw_wireframe_modifier(layout):
        col = layout.column(align=True)
        box = col.box()
        row = box.row()
        draw_show_expanded(mod,row)
        row.prop(mod,'name',text="",icon='MOD_WIREFRAME')
        draw_apply_close(row,mod.name)
        if mod.show_expanded:
            box = col.box()
            has_vgroup = bool(mod.vertex_group)
    
            split = box.split()
    
            col = split.column()
            col.prop(mod, "thickness", text="Thickness")
    
            row = col.row(align=True)
            row.prop_search(mod, "vertex_group", obj, "vertex_groups", text="")
            sub = row.row(align=True)
            sub.active = has_vgroup
            sub.prop(mod, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')
            row = col.row(align=True)
            row.active = has_vgroup
            row.prop(mod, "thickness_vertex_group", text="Factor")
    
            col.prop(mod, "use_crease", text="Crease Edges")
            col.prop(mod, "crease_weight", text="Crease Weight")
    
            col = split.column()
    
            col.prop(mod, "offset")
            col.prop(mod, "use_even_offset", text="Even Thickness")
            col.prop(mod, "use_relative_offset", text="Relative Thickness")
            col.prop(mod, "use_boundary", text="Boundary")
            col.prop(mod, "use_replace", text="Replace Original")
    
            col.prop(mod, "material_offset", text="Material Offset")                            
    
    if mod.type == 'ARRAY':
        draw_array_modifier(layout)
    elif mod.type == 'BEVEL':
        draw_bevel_modifier(layout)
    elif mod.type == 'BOOLEAN':
        draw_boolean_modifier(layout)
    elif mod.type == 'CURVE':
        draw_curve_modifier(layout)
    elif mod.type == 'DECIMATE':
        draw_decimate_modifier(layout)
    elif mod.type == 'EDGE_SPLIT':
        draw_edge_split_modifier(layout)
    elif mod.type == 'HOOK':
        draw_hook_modifier(layout)
    elif mod.type == 'MASK':
        draw_mask_modifier(layout)
    elif mod.type == 'MIRROR':
        draw_mirror_modifier(layout)
    elif mod.type == 'SOLIDIFY':
        draw_solidify_modifier(layout)
    elif mod.type == 'SUBSURF':
        draw_subsurf_modifier(layout)
    elif mod.type == 'SKIN':
        draw_skin_modifier(layout)
    elif mod.type == 'TRIANGULATE':
        draw_triangulate_modifier(layout)
    elif mod.type == 'WIREFRAME':
        draw_wireframe_modifier(layout)
    else:
        row = layout.row()
        row.label(mod.name + " view ")
    
def draw_constraint(con,layout,obj):

    def draw_show_expanded(con,layout):
        if con.show_expanded:
            layout.prop(con,'show_expanded',text="",emboss=False)
        else:
            layout.prop(con,'show_expanded',text="",emboss=False)

    def space_template(layout, con, target=True, owner=True):
        if target or owner:

            split = layout.split(percentage=0.2)

            split.label(text="Space:")
            row = split.row()

            if target:
                row.prop(con, "target_space", text="")

            if target and owner:
                row.label(icon='ARROW_LEFTRIGHT')

            if owner:
                row.prop(con, "owner_space", text="")

    def target_template(layout, con, subtargets=True):
        layout.prop(con, "target")  # XXX limiting settings for only 'curves' or some type of object

        if con.target and subtargets:
            if con.target.type == 'ARMATURE':
                layout.prop_search(con, "subtarget", con.target.data, "bones", text="Bone")

                if hasattr(con, "head_tail"):
                    row = layout.row()
                    row.label(text="Head/Tail:")
                    row.prop(con, "head_tail", text="")
            elif con.target.type in {'MESH', 'LATTICE'}:
                layout.prop_search(con, "subtarget", con.target, "vertex_groups", text="Vertex Group")

    def draw_copy_location_constraint(layout):
        col = layout.column(align=True)
        box = col.template_constraint(con)
        row = box.row()
        draw_show_expanded(con,row)
        if con.show_expanded:
            target_template(box, con)
            
            split = box.split()
    
            col = split.column()
            col.prop(con, "use_x", text="X")
            sub = col.column()
            sub.active = con.use_x
            sub.prop(con, "invert_x", text="Invert")
    
            col = split.column()
            col.prop(con, "use_y", text="Y")
            sub = col.column()
            sub.active = con.use_y
            sub.prop(con, "invert_y", text="Invert")
    
            col = split.column()
            col.prop(con, "use_z", text="Z")
            sub = col.column()
            sub.active = con.use_z
            sub.prop(con, "invert_z", text="Invert")
    
            box.prop(con, "use_offset")
    
            space_template(box, con)
            
            if con.type not in {'RIGID_BODY_JOINT', 'NULL'}:
                box.prop(con, "influence")            
     
    def draw_copy_rotation_constraint(layout):
        col = layout.column(align=True)
        box = col.template_constraint(con)
        row = box.row()
        draw_show_expanded(con,row)
        if con.show_expanded:        
            target_template(box, con)
    
            split = box.split()
    
            col = split.column()
            col.prop(con, "use_x", text="X")
            sub = col.column()
            sub.active = con.use_x
            sub.prop(con, "invert_x", text="Invert")
    
            col = split.column()
            col.prop(con, "use_y", text="Y")
            sub = col.column()
            sub.active = con.use_y
            sub.prop(con, "invert_y", text="Invert")
    
            col = split.column()
            col.prop(con, "use_z", text="Z")
            sub = col.column()
            sub.active = con.use_z
            sub.prop(con, "invert_z", text="Invert")
    
            box.prop(con, "use_offset")
    
            space_template(box, con) 
            
            if con.type not in {'RIGID_BODY_JOINT', 'NULL'}:
                box.prop(con, "influence")            
    
    def draw_copy_scale_constraint(layout):
        col = layout.column(align=True)
        box = col.template_constraint(con)
        row = box.row()
        draw_show_expanded(con,row)
        if con.show_expanded:        
            target_template(box, con)
    
            row = box.row(align=True)
            row.prop(con, "use_x", text="X")
            row.prop(con, "use_y", text="Y")
            row.prop(con, "use_z", text="Z")
    
            box.prop(con, "use_offset")
    
            space_template(box, con)
            
            if con.type not in {'RIGID_BODY_JOINT', 'NULL'}:
                box.prop(con, "influence")  
    
    def draw_copy_transforms_constraint(layout):
        col = layout.column(align=True)
        box = col.template_constraint(con)
        row = box.row()
        draw_show_expanded(con,row)
        if con.show_expanded:        
            target_template(box, con)

            space_template(box, con)
            
            if con.type not in {'RIGID_BODY_JOINT', 'NULL'}:
                box.prop(con, "influence")  
    
    def draw_limit_distance_constraint(layout):
        col = layout.column(align=True)
        box = col.template_constraint(con)
        row = box.row()
        draw_show_expanded(con,row)
        if con.show_expanded:        
            target_template(box, con)
    
            col = box.column(align=True)
            col.prop(con, "distance")
            col.operator("constraint.limitdistance_reset")
    
            row = box.row()
            row.label(text="Clamp Region:")
            row.prop(con, "limit_mode", text="")
    
            row = box.row()
            row.prop(con, "use_transform_limit")
            row.label()
    
            space_template(box, con) 
            
            if con.type not in {'RIGID_BODY_JOINT', 'NULL'}:
                box.prop(con, "influence")  
    
    def draw_limit_location_constraint(layout):
        col = layout.column(align=True)
        box = col.template_constraint(con)
        row = box.row()
        draw_show_expanded(con,row)
        if con.show_expanded:        
            split = box.split()
    
            col = split.column()
            col.prop(con, "use_min_x")
            sub = col.column()
            sub.active = con.use_min_x
            sub.prop(con, "min_x", text="")
            col.prop(con, "use_max_x")
            sub = col.column()
            sub.active = con.use_max_x
            sub.prop(con, "max_x", text="")
    
            col = split.column()
            col.prop(con, "use_min_y")
            sub = col.column()
            sub.active = con.use_min_y
            sub.prop(con, "min_y", text="")
            col.prop(con, "use_max_y")
            sub = col.column()
            sub.active = con.use_max_y
            sub.prop(con, "max_y", text="")
    
            col = split.column()
            col.prop(con, "use_min_z")
            sub = col.column()
            sub.active = con.use_min_z
            sub.prop(con, "min_z", text="")
            col.prop(con, "use_max_z")
            sub = col.column()
            sub.active = con.use_max_z
            sub.prop(con, "max_z", text="")
    
            row = box.row()
            row.prop(con, "use_transform_limit")
            row.label()
    
            row = box.row()
            row.label(text="Convert:")
            row.prop(con, "owner_space", text="")
            
            if con.type not in {'RIGID_BODY_JOINT', 'NULL'}:
                box.prop(con, "influence")  
    
    def draw_limit_rotation_constraint(layout):
        col = layout.column(align=True)
        box = col.template_constraint(con)
        row = box.row()
        draw_show_expanded(con,row)
        if con.show_expanded:        
            split = box.split()
    
            col = split.column(align=True)
            col.prop(con, "use_limit_x")
            sub = col.column(align=True)
            sub.active = con.use_limit_x
            sub.prop(con, "min_x", text="Min")
            sub.prop(con, "max_x", text="Max")
    
            col = split.column(align=True)
            col.prop(con, "use_limit_y")
            sub = col.column(align=True)
            sub.active = con.use_limit_y
            sub.prop(con, "min_y", text="Min")
            sub.prop(con, "max_y", text="Max")
    
            col = split.column(align=True)
            col.prop(con, "use_limit_z")
            sub = col.column(align=True)
            sub.active = con.use_limit_z
            sub.prop(con, "min_z", text="Min")
            sub.prop(con, "max_z", text="Max")
    
            box.prop(con, "use_transform_limit")
    
            row = box.row()
            row.label(text="Convert:")
            row.prop(con, "owner_space", text="")
            
            if con.type not in {'RIGID_BODY_JOINT', 'NULL'}:
                box.prop(con, "influence")   
    
    def draw_limit_scale_constraint(layout):
        col = layout.column(align=True)
        box = col.template_constraint(con)
        row = box.row()
        draw_show_expanded(con,row)
        if con.show_expanded:        
            split = box.split()
    
            col = split.column()
            col.prop(con, "use_min_x")
            sub = col.column()
            sub.active = con.use_min_x
            sub.prop(con, "min_x", text="")
            col.prop(con, "use_max_x")
            sub = col.column()
            sub.active = con.use_max_x
            sub.prop(con, "max_x", text="")
    
            col = split.column()
            col.prop(con, "use_min_y")
            sub = col.column()
            sub.active = con.use_min_y
            sub.prop(con, "min_y", text="")
            col.prop(con, "use_max_y")
            sub = col.column()
            sub.active = con.use_max_y
            sub.prop(con, "max_y", text="")
    
            col = split.column()
            col.prop(con, "use_min_z")
            sub = col.column()
            sub.active = con.use_min_z
            sub.prop(con, "min_z", text="")
            col.prop(con, "use_max_z")
            sub = col.column()
            sub.active = con.use_max_z
            sub.prop(con, "max_z", text="")
    
            row = box.row()
            row.prop(con, "use_transform_limit")
            row.label()
    
            row = box.row()
            row.label(text="Convert:")
            row.prop(con, "owner_space", text="")
            
            if con.type not in {'RIGID_BODY_JOINT', 'NULL'}:
                box.prop(con, "influence")                     
            
    if con.type == 'COPY_LOCATION':
        draw_copy_location_constraint(layout)
    elif con.type == 'COPY_ROTATION':
        draw_copy_rotation_constraint(layout)
    elif con.type == 'COPY_SCALE':
        draw_copy_scale_constraint(layout)
    elif con.type == 'COPY_TRANSFORMS':
        draw_copy_transforms_constraint(layout)
    elif con.type == 'LIMIT_DISTANCE':
        draw_limit_distance_constraint(layout)
    elif con.type == 'LIMIT_LOCATION':
        draw_limit_location_constraint(layout)
    elif con.type == 'LIMIT_ROTATION':
        draw_limit_rotation_constraint(layout)
    elif con.type == 'LIMIT_SCALE':
        draw_limit_scale_constraint(layout)
    else:
        row = layout.row()
        row.label(con.name + " view ")            

def draw_object_properties(layout,obj):
    ui = bpy.context.scene.mv.ui
    col = layout.column(align=True)
    box = col.box()
    row = box.row(align=True)
    draw_object_tabs(row,obj)
    box = col.box()
    col = box.column()
    if ui.interface_object_tabs == 'INFO':
        draw_object_info(col,obj)
    if ui.interface_object_tabs == 'DISPLAY':
        draw_object_display(col,obj)
    if ui.interface_object_tabs == 'MATERIAL':
        draw_object_materials(col,obj)
    if ui.interface_object_tabs == 'CONSTRAINTS':
        draw_object_constraints(col,obj)
    if ui.interface_object_tabs == 'MODIFIERS':
        draw_object_modifiers(col,obj)
    if ui.interface_object_tabs == 'MESHDATA':
        draw_object_data(col,obj)
    if ui.interface_object_tabs == 'CURVEDATA':
        draw_object_data(col,obj)
    if ui.interface_object_tabs == 'TEXTDATA':
        draw_object_data(col,obj)
    if ui.interface_object_tabs == 'EMPTYDATA':
        draw_object_data(col,obj)
    if ui.interface_object_tabs == 'LIGHTDATA':
        draw_object_data(col,obj)
    if ui.interface_object_tabs == 'CAMERADATA':
        draw_object_data(col,obj)
    if ui.interface_object_tabs == 'DRIVERS':
        draw_object_drivers(col,obj)
        
def draw_object_tabs(layout,obj):
    ui = bpy.context.scene.mv.ui
    if obj.type == 'MESH':
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[0][0], icon=const.icon_info, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[1][0], icon=const.icon_display, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[2][0], icon=const.icon_material, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[3][0], icon=const.icon_constraints, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[4][0], icon=const.icon_modifiers, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[5][0], icon=const.icon_mesh_data, text="")  
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[11][0], icon=const.icon_drivers, text="")   
    if obj.type == 'CURVE':
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[0][0], icon=const.icon_info, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[1][0], icon=const.icon_display, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[2][0], icon=const.icon_material, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[3][0], icon=const.icon_constraints, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[4][0], icon=const.icon_modifiers, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[6][0], icon=const.icon_curve_data, text="")  
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[11][0], icon=const.icon_drivers, text="")  
    if obj.type == 'FONT':
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[0][0], icon=const.icon_info, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[1][0], icon=const.icon_display, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[2][0], icon=const.icon_material, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[3][0], icon=const.icon_constraints, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[4][0], icon=const.icon_modifiers, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[7][0], icon=const.icon_font_data, text="")  
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[11][0], icon=const.icon_drivers, text="")  
    if obj.type == 'EMPTY':
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[0][0], icon=const.icon_info, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[1][0], icon=const.icon_display, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[3][0], icon=const.icon_constraints, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[8][0], icon=const.icon_empty_data, text="")  
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[11][0], icon=const.icon_drivers, text="")  
    if obj.type == 'LAMP':
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[0][0], icon=const.icon_info, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[1][0], icon=const.icon_display, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[3][0], icon=const.icon_constraints, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[9][0], icon=const.icon_light, text="")  
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[11][0], icon=const.icon_drivers, text="")  
    if obj.type == 'CAMERA':
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[0][0], icon=const.icon_info, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[3][0], icon=const.icon_constraints, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[10][0], icon=const.icon_camera, text="")  
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[11][0], icon=const.icon_drivers, text="")  
    if obj.type == 'ARMATURE':
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[0][0], icon=const.icon_info, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[1][0], icon=const.icon_display, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[3][0], icon=const.icon_constraints, text="") 
        layout.prop_enum(ui, "interface_object_tabs", enums.enum_object_tabs[11][0], icon=const.icon_drivers, text="")  

def draw_object_info(layout,obj):
    box = layout.box()
    row = box.row()
    row.prop(obj,'name')
    if obj.type in {'MESH','CURVE','LATTICE','TEXT'}:
        row.operator('fd_object.toggle_edit_mode',text="",icon='EDITMODE_HLT').object_name = obj.name
    if has_hook_modifier(obj):
        row = box.row()
        col = row.column(align=True)
        col.label("Dimension")
        col.label("X: " + str(round(meter_to_unit(obj.dimensions.x),4)))
        col.label("Y: " + str(round(meter_to_unit(obj.dimensions.y),4)))
        col.label("Z: " + str(round(meter_to_unit(obj.dimensions.z),4)))
        col = row.column(align=True)
        col.label("Location")
        col.label("X: " + str(round(meter_to_unit(obj.location.x),4)))
        col.label("Y: " + str(round(meter_to_unit(obj.location.y),4)))
        col.label("Z: " + str(round(meter_to_unit(obj.location.z),4)))
        col = row.column(align=True)
        col.label("Rotation")
        col.label("X: " + str(round(math.degrees(obj.rotation_euler.x),4)))
        col.label("Y: " + str(round(math.degrees(obj.rotation_euler.y),4)))
        col.label("Z: " + str(round(math.degrees(obj.rotation_euler.z),4)))
        box.operator("fd_object.apply_hook_modifiers",icon='HOOK').object_name = obj.name
    else:
        if obj.type not in {'EMPTY','CAMERA','LAMP'}:
            box.label('Dimensions:')
            col = box.column(align=True)
            #X
            row = col.row(align=True)
            row.prop(obj,"lock_scale",index=0,text="")
            if obj.lock_scale[0]:
                row.label("X: " + str(round(meter_to_unit(obj.dimensions.x),4)))
            else:
                row.prop(obj,"dimensions",index=0,text="X")
            #Y
            row = col.row(align=True)
            row.prop(obj,"lock_scale",index=1,text="")
            if obj.lock_scale[1]:
                row.label("Y: " + str(round(meter_to_unit(obj.dimensions.y),4)))
            else:
                row.prop(obj,"dimensions",index=1,text="Y")
            #Z
            row = col.row(align=True)
            row.prop(obj,"lock_scale",index=2,text="")
            if obj.lock_scale[2]:
                row.label("Z: " + str(round(meter_to_unit(obj.dimensions.z),4)))
            else:
                row.prop(obj,"dimensions",index=2,text="Z")
                
        col1 = box.row()
        if obj:
            col2 = col1.split()
            col = col2.column(align=True)
            col.label('Location:')
            #X
            row = col.row(align=True)
            row.prop(obj,"lock_location",index=0,text="")
            if obj.lock_location[0]:
                row.label("X: " + str(round(meter_to_unit(obj.location.x),4)))
            else:
                row.prop(obj,"location",index=0,text="X")
            #Y    
            row = col.row(align=True)
            row.prop(obj,"lock_location",index=1,text="")
            if obj.lock_location[1]:
                row.label("Y: " + str(round(meter_to_unit(obj.location.y),4)))
            else:
                row.prop(obj,"location",index=1,text="Y")
            #Z    
            row = col.row(align=True)
            row.prop(obj,"lock_location",index=2,text="")
            if obj.lock_location[2]:
                row.label("Z: " + str(round(meter_to_unit(obj.location.z),4)))
            else:
                row.prop(obj,"location",index=2,text="Z")
                
            col2 = col1.split()
            col = col2.column(align=True)
            col.label('Rotation:')
            #X
            row = col.row(align=True)
            row.prop(obj,"lock_rotation",index=0,text="")
            if obj.lock_rotation[0]:
                row.label("X: " + str(round(math.degrees(obj.rotation_euler.x),4)))
            else:
                row.prop(obj,"rotation_euler",index=0,text="X")
            #Y    
            row = col.row(align=True)
            row.prop(obj,"lock_rotation",index=1,text="")
            if obj.lock_rotation[1]:
                row.label("Y: " + str(round(math.degrees(obj.rotation_euler.y),4)))
            else:
                row.prop(obj,"rotation_euler",index=1,text="Y")
            #Z    
            row = col.row(align=True)
            row.prop(obj,"lock_rotation",index=2,text="")
            if obj.lock_rotation[2]:
                row.label("Y: " + str(round(math.degrees(obj.rotation_euler.z),4)))
            else:
                row.prop(obj,"rotation_euler",index=2,text="Z")
            
def draw_object_display(layout,obj):
    box = layout.box()
    row = box.row()
    row.prop(obj,'draw_type',expand=True)
    box.prop(obj,'hide_select')
    box.prop(obj,'hide')
    box.prop(obj,'hide_render')
    box.prop(obj,'show_x_ray',icon=const.icon_ghost,text='Show X-Ray')
    box.prop(obj.cycles_visibility,'camera',icon='CAMERA_DATA',text='Show in Viewport Render')

def draw_object_materials(layout,obj):

    if obj:
        row = layout.row()
        row.label('Material Slots:')
        row.operator("object.material_slot_add", icon='ZOOMIN', text="")
        row.operator("object.material_slot_remove", icon='ZOOMOUT', text="")
        
        row = layout.row()
        row.template_list("FD_UL_materials", "", obj, "material_slots", obj, "active_material_index", rows=1)
        
        if obj.mode == 'EDIT':
            row = layout.row(align=True)
            row.operator("object.material_slot_assign", text="Assign")
            row.operator("object.material_slot_select", text="Select")
            row.operator("object.material_slot_deselect", text="Deselect")

    if obj:
        layout.template_ID(obj, "active_material", new="material.new")

def draw_object_modifiers(layout,obj):
    row = layout.row()
    row.operator_menu_enum("fd_object.add_modifier", "type", icon='MODIFIER')
    row.operator("fd_object.collapse_all_modifiers",text="",icon='FULLSCREEN_EXIT')
    for mod in obj.modifiers:
        draw_modifier(mod,layout,obj)

def draw_object_constraints(layout,obj):
    row = layout.row()
    row.operator_menu_enum("fd_object.add_constraint", "type", icon='CONSTRAINT_DATA')
    row.operator("fd_object.collapse_all_constraints",text="",icon='FULLSCREEN_EXIT')
    for con in obj.constraints:
            draw_constraint(con,layout,obj)

def draw_object_data(layout,obj):
    
    if obj.type == 'MESH':
        obj_bp = get_assembly_bp(obj)
        box = layout.box()
        row = box.row()
        row.label("Vertex Groups:")
        row.operator("object.vertex_group_add", icon='ZOOMIN', text="")
        row.operator("object.vertex_group_remove", icon='ZOOMOUT', text="").all = False
        box.template_list("FD_UL_vgroups", "", obj, "vertex_groups", obj.vertex_groups, "active_index", rows=3)
        if len(obj.vertex_groups) > 0:
            if obj.mode == 'EDIT':
                row = box.row()
                sub = row.row(align=True)
                sub.operator("object.vertex_group_assign", text="Assign")
                sub.operator("object.vertex_group_remove_from", text="Remove")
    
                sub = row.row(align=True)
                sub.operator("object.vertex_group_select", text="Select")
                sub.operator("object.vertex_group_deselect", text="Deselect")
            else:
                group = obj.vertex_groups.active
                if obj_bp:
                    box.operator("fd_assembly.connect_meshes_to_hooks_in_assembly",text="Connect Hooks",icon=const.icon_hook).object_name = obj.name
                else:
                    box.prop(group,'name')

        key = obj.data.shape_keys
        kb = obj.active_shape_key
        
        box = layout.box()
        row = box.row()
        row.label("Shape Keys:")
        row.operator("object.shape_key_add", icon='ZOOMIN', text="").from_mix = False
        row.operator("object.shape_key_remove", icon='ZOOMOUT', text="").all = False
        box.template_list("MESH_UL_shape_keys", "", key, "key_blocks", obj, "active_shape_key_index", rows=3)
        if kb and obj.active_shape_key_index != 0:
            box.prop(kb,'name')
            if obj.mode != 'EDIT':
                row = box.row()
                row.prop(kb, "value")
        
    if obj.type == 'EMPTY':
        box = layout.box()
        box.label("Empty Data",icon=const.icon_font_data)
        box.prop(obj,'empty_draw_type',text='Draw Type')
        box.prop(obj,'empty_draw_size')
        
    if obj.type == 'CURVE':
        box = layout.box()
        box.label("Curve Data",icon=const.icon_curve_data)
        curve = obj.data
        box.prop(curve,"dimensions")
        box.prop(curve,"bevel_object")
        box.prop(curve,"bevel_depth")
        box.prop(curve,"extrude")
        box.prop(curve,"use_fill_caps")
        box.prop(curve.splines[0],"use_cyclic_u")         
    
    if obj.type == 'FONT':
        text = obj.data
        box = layout.box()
        row = box.row()
        row.prop(obj.mv,"use_as_item_number")
        row = box.row()
        row.prop(obj.mv,"use_as_xdim_notation")
        row = box.row()
        row.prop(obj.mv,"use_as_ydim_notation")
        row = box.row()
        row.prop(obj.mv,"use_as_zdim_notation")
        box = layout.box()
        row = box.row()
        row.label("Font Data:")
        if obj.mode == 'OBJECT':
            row.operator("fd_object.toggle_edit_mode",text="Edit Text",icon=const.icon_edit_text).object_name = obj.name
        else:
            row.operator("fd_object.toggle_edit_mode",text="Exit Edit Mode",icon=const.icon_edit_text).object_name = obj.name
        row = box.row()
        row.template_ID(text, "font", open="font.open", unlink="font.unlink")
        row = box.row()
        row.label("Text Size:")
        row.prop(text,"size",text="")
        row = box.row()
        row.prop(text,"align")
        
        box = layout.box()
        row = box.row()
        row.label("3D Font:")
        row = box.row()
        row.prop(text,"extrude")
        row = box.row()
        row.prop(text,"bevel_depth")
        
    if obj.type == 'LAMP':
        box = layout.box()
        lamp = obj.data
        clamp = lamp.cycles
        cscene = bpy.context.scene.cycles  
        
        emissionNode = None
        mathNode = None
        
        if "Emission" in lamp.node_tree.nodes:
            emissionNode = lamp.node_tree.nodes["Emission"]
        if "Math" in lamp.node_tree.nodes:
            mathNode = lamp.node_tree.nodes["Math"]

        type_box = box.box()
        type_box.label("Lamp Type:")     
        row = type_box.row()
        row.prop(lamp, "type", expand=True)
        
        if lamp.type in {'POINT', 'SUN', 'SPOT'}:
            type_box.prop(lamp, "shadow_soft_size", text="Shadow Size")
        elif lamp.type == 'AREA':
            type_box.prop(lamp, "shape", text="")
            sub = type_box.column(align=True)

            if lamp.shape == 'SQUARE':
                sub.prop(lamp, "size")
            elif lamp.shape == 'RECTANGLE':
                sub.prop(lamp, "size", text="Size X")
                sub.prop(lamp, "size_y", text="Size Y")

        if cscene.progressive == 'BRANCHED_PATH':
            type_box.prop(clamp, "samples")

        if lamp.type == 'HEMI':
            type_box.label(text="Not supported, interpreted as sun lamp")         

        options_box = box.box()
        options_box.label("Lamp Options:")
        if emissionNode:
            row = options_box.row()
            split = row.split(percentage=0.3)
            split.label("Lamp Color:")
            split.prop(emissionNode.inputs[0],"default_value",text="")  
            
        row = options_box.row()
        split = row.split(percentage=0.3)
        split.label("Lamp Strength:")            
        if mathNode:   
            split.prop(mathNode.inputs[0],"default_value",text="") 
        else:          
            split.prop(emissionNode.inputs[1], "default_value",text="")
            
        row = options_box.row()        
        split = row.split(percentage=0.4)     
        split.prop(clamp, "cast_shadow",text="Cast Shadows")
        split.prop(clamp, "use_multiple_importance_sampling")
            
    if obj.type == 'CAMERA':
        box = layout.box()
        cam = obj.data
        ccam = cam.cycles
        scene = bpy.context.scene
        rd = scene.render
        
        box.label("Camera Options:")           
        cam_opt_box_1 = box.box()
        row = cam_opt_box_1.row(align=True)
        row.label(text="Render Size:",icon='STICKY_UVS_VERT')        
        row.prop(rd, "resolution_x", text="X")
        row.prop(rd, "resolution_y", text="Y")
        cam_opt_box_1.prop(cam, "type", expand=False, text="Camera Type")
        split = cam_opt_box_1.split()
        col = split.column()
        if cam.type == 'PERSP':
            row = col.row()
            if cam.lens_unit == 'MILLIMETERS':
                row.prop(cam, "lens")
            elif cam.lens_unit == 'FOV':
                row.prop(cam, "angle")
            row.prop(cam, "lens_unit", text="")

        if cam.type == 'ORTHO':
            col.prop(cam, "ortho_scale")

        if cam.type == 'PANO':
            engine = bpy.context.scene.render.engine
            if engine == 'CYCLES':
                ccam = cam.cycles
                col.prop(ccam, "panorama_type", text="Panorama Type")
                if ccam.panorama_type == 'FISHEYE_EQUIDISTANT':
                    col.prop(ccam, "fisheye_fov")
                elif ccam.panorama_type == 'FISHEYE_EQUISOLID':
                    row = col.row()
                    row.prop(ccam, "fisheye_lens", text="Lens")
                    row.prop(ccam, "fisheye_fov")
            elif engine == 'BLENDER_RENDER':
                row = col.row()
                if cam.lens_unit == 'MILLIMETERS':
                    row.prop(cam, "lens")
                elif cam.lens_unit == 'FOV':
                    row.prop(cam, "angle")
                row.prop(cam, "lens_unit", text="")
        
        row = cam_opt_box_1.row()
#         row.menu("CAMERA_MT_presets", text=bpy.types.CAMERA_MT_presets.bl_label)         
        row.prop_menu_enum(cam, "show_guide")            
        row = cam_opt_box_1.row()
        split = row.split()
        col = split.column()
        col.prop(cam, "clip_start", text="Clipping Start")
        col.prop(cam, "clip_end", text="Clipping End")      
        col = row.column()         
        col.prop(bpy.context.scene.cycles,"film_transparent",text="Transparent Film")   
        
        box.label(text="Depth of Field:")
        dof_box = box.box()
        row = dof_box.row()
        row.label("Focus:")
        row = dof_box.row(align=True)
        row.prop(cam, "dof_object", text="")
        col = row.column()
        sub = col.row()
        sub.active = cam.dof_object is None
        sub.prop(cam, "dof_distance", text="Distance")
        
def draw_object_drivers(layout,obj):
    if obj:
        if not obj.animation_data:
            layout.label("There are no drivers assigned to the object",icon='ERROR')
        else:
            if len(obj.animation_data.drivers) == 0:
                layout.label("There are no drivers assigned to the object",icon='ERROR')
            for DR in obj.animation_data.drivers:
                box = layout.box()
                row = box.row()
                DriverName = DR.data_path
                if DriverName == "location" or DriverName == "rotation_euler" or DriverName == "dimensions":
                    if DR.array_index == 0:
                        DriverName = DriverName + " X"
                    if DR.array_index == 1:
                        DriverName = DriverName + " Y"
                    if DR.array_index == 2:
                        DriverName = DriverName + " Z"                     
                value = eval('bpy.data.objects["' + obj.name + '"].' + DR.data_path)
                if isinstance(value,type(obj.location)):
                    value = value[DR.array_index]
#                 row.label(DriverName + " = " + str(round(meter_to_unit(value),5)),icon='AUTO')
                row.label(DriverName + " = " + str(value),icon='AUTO')
                props = row.operator("fd_driver.add_variable_to_object",text="",icon='ZOOMIN')
                props.object_name = obj.name
                props.data_path = DR.data_path
                props.array_index = DR.array_index
                obj_bp = get_assembly_bp(obj)
                if obj_bp:
                    props = row.operator('fd_driver.get_vars_from_object',text="",icon='DRIVER')
                    props.object_name = obj.name
                    props.var_object_name = obj_bp.name
                    props.data_path = DR.data_path
                    props.array_index = DR.array_index
                draw_driver_expression(box,DR)
#                 draw_add_variable_operators(box,obj.name,DR.data_path,DR.array_index)
                draw_driver_variables(box,DR,obj.name)

def draw_driver_expression(layout,driver):
    row = layout.row(align=True)
    row.prop(driver.driver,'show_debug_info',text="",icon='OOPS')
    if driver.driver.is_valid:
        row.prop(driver.driver,"expression",text="",expand=True,icon='FILE_TICK')
        if driver.mute:
            row.prop(driver,"mute",text="",icon='OUTLINER_DATA_SPEAKER')
        else:
            row.prop(driver,"mute",text="",icon='OUTLINER_OB_SPEAKER')
    else:
        row.prop(driver.driver,"expression",text="",expand=True,icon='ERROR')
        if driver.mute:
            row.prop(driver,"mute",text="",icon='OUTLINER_DATA_SPEAKER')
        else:
            row.prop(driver,"mute",text="",icon='OUTLINER_OB_SPEAKER')

def draw_driver_variables(layout,driver,object_name):
    
    for var in driver.driver.variables:
        col = layout.column()
        boxvar = col.box()
        row = boxvar.row(align=True)
        row.prop(var,"name",text="",icon='FORWARD')
        
        props = row.operator("fd_driver.remove_variable",text="",icon='X',emboss=False)
        props.object_name = object_name
        props.data_path = driver.data_path
        props.array_index = driver.array_index
        props.var_name = var.name
        for target in var.targets:
            if driver.driver.show_debug_info:
                row = boxvar.row()
                row.prop(var,"type",text="")
                row = boxvar.row()
                row.prop(target,"id",text="")
                row = boxvar.row(align=True)
                row.prop(target,"data_path",text="",icon='ANIM_DATA')
            if target.id:
                value = eval('bpy.data.objects["' + target.id.name + '"]'"." + target.data_path)
            else:
                value = "ERROR#"
            
            row = boxvar.row()
            row.label("",icon='BLANK1')
            row.label("",icon='BLANK1')
#             row.label("Value: " + str(round(meter_to_unit(value),5)))
            row.label("Value: " + str(value))

def draw_add_variable_operators(layout,object_name,data_path,array_index):
    #GLOBAL PROMPT
    obj = bpy.data.objects[object_name]
    obj_bp = get_assembly_bp(obj)
    box = layout.box()
    box.label("Quick Variables",icon='DRIVER')
    row = box.row()
    if obj_bp:
        props = row.operator('fd_driver.get_vars_from_object',text="Group Variables",icon='DRIVER')
        props.object_name = object_name
        props.var_object_name = obj_bp.name
        props.data_path = data_path
        props.array_index = array_index
