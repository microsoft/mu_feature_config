## @ ConfigEditor.py
#
# Copyright (c) 2018 - 2020, Intel Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import os
import sys
import marshal
import base64
from pathlib import Path

sys.dont_write_bytecode = True

import tkinter                                                  # noqa: E402
import tkinter.ttk as ttk                                       # noqa: E402
import tkinter.messagebox as messagebox                         # noqa: E402
import tkinter.filedialog as filedialog                         # noqa: E402
from SettingSupport.SettingsXMLLib import SettingsXMLLib        # noqa: E402
from GenNCCfgData import CGenNCCfgData                          # noqa: E402
from GenCfgData import (                                        # noqa: E402
    CGenCfgData,
    bytes_to_value,
    bytes_to_bracket_str,
    value_to_bytes,
    array_str_to_value,
)


class create_tool_tip(object):
    """
    create a tooltip for a given widget
    """

    in_progress = False

    def __init__(self, widget, text=""):
        self.top_win = None
        self.widget = widget
        self.text = text
        self.widget.bind("<Enter>", self.enter)
        self.widget.bind("<Leave>", self.leave)

    def enter(self, event=None):
        if self.in_progress:
            return
        if self.widget.winfo_class() == "Treeview":
            # Only show help when cursor is on row header.
            rowid = self.widget.identify_row(event.y)
            if rowid != "":
                return
        else:
            x, y, cx, cy = self.widget.bbox("insert")

        cursor = self.widget.winfo_pointerxy()
        x = self.widget.winfo_rootx() + 35
        y = self.widget.winfo_rooty() + 20
        if cursor[1] > y and cursor[1] < y + 20:
            y += 20

        # creates a toplevel window
        self.top_win = tkinter.Toplevel(self.widget)
        # Leaves only the label and removes the app window
        self.top_win.wm_overrideredirect(True)
        self.top_win.wm_geometry("+%d+%d" % (x, y))
        label = tkinter.Message(
            self.top_win,
            text=self.text,
            justify="left",
            background="bisque",
            relief="solid",
            borderwidth=1,
            font=("times", "10", "normal"),
        )
        label.pack(ipadx=1)
        self.in_progress = True

    def leave(self, event=None):
        if self.top_win:
            self.top_win.destroy()
            self.in_progress = False


class validating_entry(tkinter.Entry):
    def __init__(self, master, **kw):
        tkinter.Entry.__init__(*(self, master), **kw)
        self.parent = master
        self.old_value = ""
        self.last_value = ""
        self.variable = tkinter.StringVar()
        self.variable.trace("w", self.callback)
        self.config(textvariable=self.variable)
        self.config({"background": "#c0c0c0"})
        self.bind("<Return>", self.move_next)
        self.bind("<Tab>", self.move_next)
        self.bind("<Escape>", self.cancel)
        for each in ["BackSpace", "Delete"]:
            self.bind("<%s>" % each, self.ignore)
        self.display(None)

    def ignore(self, even):
        return "break"

    def move_next(self, event):
        if self.row < 0:
            return
        row, col = self.row, self.col
        txt, row_id, col_id = self.parent.get_next_cell(row, col)
        self.display(txt, row_id, col_id)
        return "break"

    def cancel(self, event):
        self.variable.set(self.old_value)
        self.display(None)

    def display(self, txt, row_id="", col_id=""):
        if txt is None:
            self.row = -1
            self.col = -1
            self.place_forget()
        else:
            row = int("0x" + row_id[1:], 0) - 1
            col = int(col_id[1:]) - 1
            self.row = row
            self.col = col
            self.old_value = txt
            self.last_value = txt
            x, y, width, height = self.parent.bbox(row_id, col)
            self.place(x=x, y=y, w=width)
            self.variable.set(txt)
            self.focus_set()
            self.icursor(0)

    def callback(self, *Args):
        cur_val = self.variable.get()
        new_val = self.validate(cur_val)
        if new_val is not None and self.row >= 0:
            self.last_value = new_val
            self.parent.set_cell(self.row, self.col, new_val)
        self.variable.set(self.last_value)

    def validate(self, value):
        if len(value) > 0:
            try:
                int(value, 16)
            except:
                return None

        # Normalize the cell format
        self.update()
        cell_width = self.winfo_width()
        max_len = custom_table.to_byte_length(cell_width) * 2
        cur_pos = self.index("insert")
        if cur_pos == max_len + 1:
            value = value[-max_len:]
        else:
            value = value[:max_len]
        if value == "":
            value = "0"
        fmt = "%%0%dX" % max_len
        return fmt % int(value, 16)


class custom_table(ttk.Treeview):
    _Padding = 20
    _Char_width = 6

    def __init__(self, parent, col_hdr, bins):
        cols = len(col_hdr)

        col_byte_len = []
        for col in range(cols):  # Columns
            col_byte_len.append(int(col_hdr[col].split(":")[1]))

        byte_len = sum(col_byte_len)
        rows = (len(bins) + byte_len - 1) // byte_len

        self.rows = rows
        self.cols = cols
        self.col_byte_len = col_byte_len
        self.col_hdr = col_hdr

        self.size = len(bins)
        self.last_dir = ""

        style = ttk.Style()
        style.configure(
            "Custom.Treeview.Heading", font=("calibri", 10, "bold"), foreground="blue"
        )
        ttk.Treeview.__init__(
            self,
            parent,
            height=rows,
            columns=[""] + col_hdr,
            show="headings",
            style="Custom.Treeview",
            selectmode="none",
        )
        self.bind("<Button-1>", self.click)
        self.bind("<FocusOut>", self.focus_out)
        self.entry = validating_entry(self, width=4, justify=tkinter.CENTER)

        self.heading(0, text="LOAD")
        self.column(0, width=60, stretch=0, anchor=tkinter.CENTER)

        for col in range(cols):  # Columns
            text = col_hdr[col].split(":")[0]
            byte_len = int(col_hdr[col].split(":")[1])
            self.heading(col + 1, text=text)
            self.column(
                col + 1,
                width=self.to_cell_width(byte_len),
                stretch=0,
                anchor=tkinter.CENTER,
            )

        idx = 0
        for row in range(rows):  # Rows
            text = "%04X" % (row * len(col_hdr))
            vals = ["%04X:" % (cols * row)]
            for col in range(cols):  # Columns
                if idx >= len(bins):
                    break
                byte_len = int(col_hdr[col].split(":")[1])
                value = bytes_to_value(bins[idx: idx + byte_len])
                hex = ("%%0%dX" % (byte_len * 2)) % value
                vals.append(hex)
                idx += byte_len
            self.insert("", "end", values=tuple(vals))
            if idx >= len(bins):
                break

    @staticmethod
    def to_cell_width(byte_len):
        return byte_len * 2 * custom_table._Char_width + custom_table._Padding

    @staticmethod
    def to_byte_length(cell_width):
        return (cell_width - custom_table._Padding) // (2 * custom_table._Char_width)

    def focus_out(self, event):
        self.entry.display(None)

    def refresh_bin(self, bins):
        if not bins:
            return

        # Reload binary into widget
        bin_len = len(bins)
        for row in range(self.rows):
            iid = self.get_children()[row]
            for col in range(self.cols):
                idx = row * sum(self.col_byte_len) + sum(self.col_byte_len[:col])
                byte_len = self.col_byte_len[col]
                if idx + byte_len <= self.size:
                    byte_len = int(self.col_hdr[col].split(":")[1])
                    if idx + byte_len > bin_len:
                        val = 0
                    else:
                        val = bytes_to_value(bins[idx: idx + byte_len])
                    hex_val = ("%%0%dX" % (byte_len * 2)) % val
                    self.set(iid, col + 1, hex_val)

    def get_cell(self, row, col):
        iid = self.get_children()[row]
        txt = self.item(iid, "values")[col]
        return txt

    def get_next_cell(self, row, col):
        rows = self.get_children()
        col += 1
        if col > self.cols:
            col = 1
            row += 1
        cnt = row * sum(self.col_byte_len) + sum(self.col_byte_len[:col])
        if cnt > self.size:
            # Reached the last cell, so roll back to beginning
            row = 0
            col = 1

        txt = self.get_cell(row, col)
        row_id = rows[row]
        col_id = "#%d" % (col + 1)
        return (txt, row_id, col_id)

    def set_cell(self, row, col, val):
        iid = self.get_children()[row]
        self.set(iid, col, val)

    def load_bin(self):
        # Load binary from file
        path = filedialog.askopenfilename(
            initialdir=self.last_dir,
            title="Load binary file",
            filetypes=(("Binary files", "*.bin"), ("binary files", "*.bin")),
        )
        if path:
            self.last_dir = os.path.dirname(path)
            fd = open(path, "rb")
            bins = bytearray(fd.read())[: self.size]
            fd.close()
            bins.extend(b"\x00" * (self.size - len(bins)))
            return bins

        return None

    def click(self, event):
        row_id = self.identify_row(event.y)
        col_id = self.identify_column(event.x)
        if row_id == "" and col_id == "#1":
            # Clicked on "LOAD" cell
            bins = self.load_bin()
            self.refresh_bin(bins)
            return

        if col_id == "#1":
            # Clicked on column 1 (Offset column)
            return

        item = self.identify("item", event.x, event.y)
        if not item or not col_id:
            # Not clicked on valid cell
            return

        # Clicked cell
        row = int("0x" + row_id[1:], 0) - 1
        col = int(col_id[1:]) - 1
        if row * self.cols + col > self.size:
            return

        vals = self.item(item, "values")
        if col < len(vals):
            txt = self.item(item, "values")[col]
            self.entry.display(txt, row_id, col_id)

    def get(self):
        bins = bytearray()
        row_ids = self.get_children()
        for row_id in row_ids:
            row = int("0x" + row_id[1:], 0) - 1
            for col in range(self.cols):
                idx = row * sum(self.col_byte_len) + sum(self.col_byte_len[:col])
                byte_len = self.col_byte_len[col]
                if idx + byte_len > self.size:
                    break
                hex = self.item(row_id, "values")[col + 1]
                values = value_to_bytes(
                    int(hex, 16) & ((1 << byte_len * 8) - 1), byte_len
                )
                bins.extend(values)
        return bins


class state:
    def __init__(self):
        self.state = False

    def set(self, value):
        self.state = value

    def get(self):
        return self.state


class cfg_data:
    def __init__(self):
        self.cfg_data_obj = None
        self.org_cfg_data_bin = None
        self.config_type = ''


class application(tkinter.Frame):
    def __init__(self, master=None):
        root = master

        self.debug = True
        self.page_id = ""
        self.page_list = {}
        self.conf_list = {}
        self.in_left = state()
        self.in_right = state()
        self.cfg_data_list = {}
        # this maps page id to cfg_data index, needed for when user changes pages
        # self.page_cfg_map[page_id] = cfg_data_idx
        self.page_cfg_map = {}

        # Check if current directory contains a file with a .yaml extension
        # if not default self.last_dir to a Platform directory where it is
        # easier to locate *BoardPkg\CfgData\*Def.yaml files
        self.last_dir = "."
        if not any(fname.endswith(".yaml") for fname in os.listdir(".")):
            platform_path = (
                Path(os.path.realpath(__file__)).parents[2].joinpath("Platform")
            )
            if platform_path.exists():
                self.last_dir = platform_path

        tkinter.Frame.__init__(self, master, borderwidth=2)

        self.menu_string = [
            "Save Config Data to Var List Binary",
            "Load Config Data from Var List Binary",
            'Load Config from Change File',
            'Save Full Config Data to SVD File',
            'Save Config Changes to SVD File',
            'Load Config Changes from SVD File',
            'Save Config Changes to Change File',
            'Save Full Config Data to Change File',
            "Save Config Data to Raw Binary",
            "Load Config Data from Raw Binary"
        ]

        self.yaml_specific_setting = [
            "Save Config Data to Raw Binary",
            "Load Config Data from Raw Binary",
            'Save Config Changes to SVD File',
        ]

        root.geometry("1200x800")

        paned = ttk.Panedwindow(root, orient=tkinter.HORIZONTAL)
        paned.pack(fill=tkinter.BOTH, expand=True, padx=(4, 4))

        status = tkinter.Label(
            master, text="", bd=1, relief=tkinter.SUNKEN, anchor=tkinter.W
        )
        status.pack(side=tkinter.BOTTOM, fill=tkinter.X)

        frame_left = ttk.Frame(paned, height=800, relief="groove")

        self.left = ttk.Treeview(frame_left, show="tree")

        # Set up tree HScroller
        pady = (10, 10)
        self.tree_scroll = ttk.Scrollbar(
            frame_left, orient="vertical", command=self.left.yview
        )
        self.left.configure(yscrollcommand=self.tree_scroll.set)
        self.left.bind("<<TreeviewSelect>>", self.on_config_page_select_change)
        self.left.bind("<Enter>", lambda e: self.in_left.set(True))
        self.left.bind("<Leave>", lambda e: self.in_left.set(False))
        self.left.bind("<MouseWheel>", self.on_tree_scroll)

        self.left.pack(
            side="left", fill=tkinter.BOTH, expand=True, padx=(5, 0), pady=pady
        )
        self.tree_scroll.pack(side="right", fill=tkinter.Y, pady=pady, padx=(0, 5))

        frame_right = ttk.Frame(paned, relief="groove")
        self.frame_right = frame_right

        self.conf_canvas = tkinter.Canvas(frame_right, highlightthickness=0)
        self.page_scroll = ttk.Scrollbar(
            frame_right, orient="vertical", command=self.conf_canvas.yview
        )
        self.right_grid = ttk.Frame(self.conf_canvas)
        self.conf_canvas.configure(yscrollcommand=self.page_scroll.set)
        self.conf_canvas.pack(
            side="left", fill=tkinter.BOTH, expand=True, pady=pady, padx=(5, 0)
        )
        self.page_scroll.pack(side="right", fill=tkinter.Y, pady=pady, padx=(0, 5))
        self.conf_canvas.create_window(0, 0, window=self.right_grid, anchor="nw")
        self.conf_canvas.bind("<Enter>", lambda e: self.in_right.set(True))
        self.conf_canvas.bind("<Leave>", lambda e: self.in_right.set(False))
        self.conf_canvas.bind("<Configure>", self.on_canvas_configure)
        self.conf_canvas.bind_all("<MouseWheel>", self.on_page_scroll)

        paned.add(frame_left, weight=2)
        paned.add(frame_right, weight=10)

        style = ttk.Style()
        style.layout("Treeview", [("Treeview.treearea", {"sticky": "nswe"})])

        menubar = tkinter.Menu(root)
        file_menu = tkinter.Menu(menubar, tearoff=0)
        file_menu.add_command(
            label="Open Config file...", command=self.load_from_ml
        )
        file_menu.add_command(
            label=self.menu_string[8], command=self.save_to_raw_bin, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[9], command=self.load_from_raw_bin, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[0], command=self.save_to_var_list_bin, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[1], command=self.load_from_var_list_bin, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[2], command=self.load_from_delta, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[3], command=self.save_full_to_svd, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[4], command=self.save_to_svd, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[5], command=self.load_from_svd, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[6], command=self.save_delta_file, state="disabled"
        )
        file_menu.add_command(
            label=self.menu_string[7], command=self.save_full_to_delta, state="disabled"
        )
        file_menu.add_command(label="About", command=self.about)
        menubar.add_cascade(label="File", menu=file_menu)
        self.file_menu = file_menu

        root.config(menu=menubar)

        if len(sys.argv) > 1:
            path = sys.argv[1]
            if not path.endswith('.yaml') and not path.endswith('.yml') \
                    and not path.endswith('.pkl') and not path.endswith('.xml'):
                messagebox.showerror('LOADING ERROR', "Unsupported file '%s' !" % path)
                return
            else:
                self.load_cfg_file(path, 0)

        for i in range(2, len(sys.argv)):
            path = sys.argv[i]
            if path.endswith(".dlt") or path.endswith(".csv"):
                self.load_delta_file(path)
            elif path.endswith(".bin,raw"):
                self.load_bin_file(path, False)
            elif path.endswith(".bin,varlist"):
                self.load_bin_file(path, True)
            elif path.endswith(".xml") or path.endswith(".yaml") or path.endswith(".yml"):
                self.load_cfg_file(path, 1)
            else:
                messagebox.showerror("LOADING ERROR", "Unsupported file '%s' !" % path)
                return

    def set_object_name(self, widget, name, file_id):
        # associate the name of the widget with the file it came from, in case of name conflicts
        self.conf_list[id(widget)] = (name, file_id)

    def get_object_name(self, widget):
        if id(widget) in self.conf_list:
            return self.conf_list[id(widget)]
        else:
            return None, None

    def limit_entry_size(self, variable, limit):
        value = variable.get()
        if len(value) > limit:
            variable.set(value[:limit])

    def on_canvas_configure(self, event):
        self.right_grid.grid_columnconfigure(0, minsize=event.width)

    def on_tree_scroll(self, event):
        if not self.in_left.get() and self.in_right.get():
            # This prevents scroll event from being handled by both left and
            # right frame at the same time.
            self.on_page_scroll(event)
            return "break"

    def on_page_scroll(self, event):
        if self.in_right.get():
            # Only scroll when it is in active area
            min, max = self.page_scroll.get()
            if not ((min == 0.0) and (max == 1.0)):
                self.conf_canvas.yview_scroll(-1 * int(event.delta / 120), "units")

    def update_visibility_for_widget(self, widget, args):

        visible = True
        item = self.get_config_data_item_from_widget(widget, True)
        if item is None:
            return visible
        elif not item:
            return visible

        file_id = self.get_object_name(widget)[1]

        result = 1
        if 'condition' in item and item['condition']:
            result = self.evaluate_condition(item, file_id)
            if result == 2:
                # Gray
                if not isinstance(widget, custom_table):
                    widget.configure(state="disabled")
            elif result == 0:
                # Hide
                visible = False
                widget.grid_remove()
            else:
                # Show
                widget.grid()
                if not isinstance(widget, custom_table):
                    widget.configure(state="normal")

        return visible

    def update_widgets_visibility_on_page(self):
        self.walk_widgets_in_layout(self.right_grid, self.update_visibility_for_widget)

    def combo_select_changed(self, event):
        self.update_config_data_from_widget(event.widget, None)
        self.update_widgets_visibility_on_page()

    def edit_num_finished(self, event):
        widget = event.widget
        item = self.get_config_data_item_from_widget(widget)
        if not item:
            return
        parts = item["type"].split(",")
        file_id = self.get_object_name(widget)[1]
        if len(parts) > 3:
            min = parts[2].lstrip()[1:]
            max = parts[3].rstrip()[:-1]
            min_val = array_str_to_value(min)
            max_val = array_str_to_value(max)
            text = widget.get()
            if "," in text:
                text = "{ %s }" % text
            try:
                value = array_str_to_value(text)
                if value < min_val or value > max_val:
                    raise Exception("Invalid input!")
                self.set_config_item_value(item, text, file_id)
            except:
                pass

            text = item["value"].strip("{").strip("}").strip()
            widget.delete(0, tkinter.END)
            widget.insert(0, text)

        self.update_widgets_visibility_on_page()

    def update_page_scroll_bar(self):
        # Update scrollbar
        self.frame_right.update()
        self.conf_canvas.config(scrollregion=self.conf_canvas.bbox("all"))

    def on_config_page_select_change(self, event):
        self.update_config_data_on_page()
        sel = self.left.selection()
        if len(sel) > 0:
            page_id = sel[0]
            self.build_config_data_page(page_id)
            self.update_widgets_visibility_on_page()
            self.update_page_scroll_bar()

    def walk_widgets_in_layout(self, parent, callback_function, args=None):
        for widget in parent.winfo_children():
            callback_function(widget, args)

    def clear_widgets_inLayout(self, parent=None):
        if parent is None:
            parent = self.right_grid

        for widget in parent.winfo_children():
            widget.destroy()

        parent.grid_forget()
        self.conf_list.clear()

    def build_config_page_tree(self, cfg_page, parent, file_id):
        for page in cfg_page["child"]:
            page_id = next(iter(page))
            # Put CFG items into related page list
            self.page_cfg_map[page_id] = file_id
            self.page_list[page_id] = self.cfg_data_list[file_id].cfg_data_obj.get_cfg_list(page_id)
            self.page_list[page_id].sort(key=lambda x: x["order"])
            page_name = self.cfg_data_list[file_id].cfg_data_obj.get_page_title(page_id)
            child = self.left.insert(
                parent, "end", iid=page_id, text=page_name, value=0
            )
            if len(page[page_id]) > 0:
                self.build_config_page_tree(page[page_id], child, file_id)

    def is_config_data_loaded(self):
        return True if len(self.page_list) else False

    def set_current_config_page(self, page_id):
        self.page_id = page_id

    def get_current_config_page(self):
        return self.page_id

    def get_current_config_data(self):
        page_id = self.get_current_config_page()
        if page_id in self.page_list:
            return self.page_list[page_id]
        else:
            return []

    def build_config_data_page(self, page_id):
        self.clear_widgets_inLayout()
        self.set_current_config_page(page_id)
        disp_list = []
        for item in self.get_current_config_data():
            disp_list.append(item)
        row = 0
        disp_list.sort(key=lambda x: x["order"])
        for item in disp_list:
            self.add_config_item(item, row, self.page_cfg_map[page_id])
            row += 2

    def load_config_data(self, file_name):
        gen_cfg_data = CGenCfgData()
        if file_name.endswith(".pkl"):
            with open(file_name, "rb") as pkl_file:
                gen_cfg_data.__dict__ = marshal.load(pkl_file)
            gen_cfg_data.prepare_marshal(False)
        elif file_name.endswith(".yaml") or file_name.endswith('.yml'):
            if gen_cfg_data.load_yaml(file_name, shallow_load=True) != 0:
                raise Exception(gen_cfg_data.get_last_error())
            nl = file_name.split(".")
            nl[-2] += "_UI"
            ui_file_name = ".".join(nl)
            if os.path.isfile(ui_file_name):
                ui_gen_cfg_data = CGenCfgData()
                if ui_gen_cfg_data.load_yaml(ui_file_name, shallow_load=True) != 0:
                    raise Exception(ui_gen_cfg_data.get_last_error())
                # Merge the UI cfg and data cfg objects
                merged_cfg_tree = gen_cfg_data.merge_cfg_tree(
                    gen_cfg_data.get_cfg_tree(), ui_gen_cfg_data.get_cfg_tree()
                )
                gen_cfg_data.set_cfg_tree(merged_cfg_tree)
            gen_cfg_data.build_cfg_list({'offset': 0})
            gen_cfg_data.build_var_dict()
            gen_cfg_data.update_def_value()
        elif file_name.endswith('.xml'):
            gen_cfg_data = CGenNCCfgData()
            if gen_cfg_data.load_xml(file_name) != 0:
                raise Exception(gen_cfg_data.get_last_error())
        else:
            raise Exception('Unsupported file "%s" !' % file_name)
        return gen_cfg_data

    def about(self):
        msg = (
            "Configuration Editor\n--------------------------------\nVersion 0.8\n2020"
        )
        lines = msg.split("\n")
        width = 30
        text = []
        for line in lines:
            text.append(line.center(width, " "))
        messagebox.showinfo("Config Editor", "\n".join(text))

    def update_last_dir(self, path):
        self.last_dir = os.path.dirname(path)

    def get_open_file_name(self, ftype):
        if self.is_config_data_loaded():
            if ftype == "dlt":
                question = ""
            elif ftype == "bin":
                question = 'All configuration will be reloaded from binary blob, continue ?'
            elif ftype == 'svd':
                question = ''
            elif 'yaml' in ftype or 'yml' in ftype or 'xml' in ftype:
                question = ''
            else:
                raise Exception("Unsupported file type !")
            if question:
                reply = messagebox.askquestion("", question, icon="warning")
                if reply == "no":
                    return None

        file_type = ''
        file_ext = ''
        if 'yaml' in ftype or 'yml' in ftype:
            file_type = 'YAML PKL'
            file_ext = 'pkl Def.yaml'
        if 'xml' in ftype:
            file_type += ' XML'
            file_ext += ' xml'
        if 'xml' not in ftype and 'yaml' not in ftype and 'yml' not in ftype:
            file_type = ftype.upper()
            file_ext = ftype

        file_ext = file_ext.split(' ')
        file_ext_opt = ['*.' + i for i in file_ext]
        path = filedialog.askopenfilename(
            initialdir=self.last_dir,
            title="Load file",
            filetypes=(("%s files" % file_type, file_ext_opt), (
                "all files", "*.*")))
        if path:
            self.update_last_dir(path)
            return path
        else:
            return None

    def load_from_delta(self):
        path = self.get_open_file_name("dlt")
        if not path:
            return
        self.load_delta_file(path)

    def load_delta_file(self, path):
        # assumption is that there is only one yaml file and one delta file
        # while is applied to that yaml file and one xml file and one csv applied
        # to that xml file
        file_id = -1
        yml_id = -1
        xml_id = -1
        is_variable_list_format = True
        for idx in self.cfg_data_list:
            if self.cfg_data_list[idx].config_type == 'yml':
                yml_id = idx
            else:
                xml_id = idx

        if path.endswith('.dlt'):
            file_id = yml_id
            is_variable_list_format = False
        elif path.endswith('.csv'):
            file_id = xml_id
        else:
            raise Exception('Unsupported file "%s" !' % path)

        # if not found_yml:
        # we didn't find a yml file, cannot apply delta file
        # messagebox.showerror("LOADING ERROR", "Could not find YAML file to apply delta to")
        # return
        self.reload_config_data_from_bin(self.cfg_data_list[file_id].org_cfg_data_bin, file_id, is_variable_list_format)
        try:
            self.cfg_data_list[file_id].cfg_data_obj.override_default_value(path)
        except Exception as e:
            messagebox.showerror("LOADING ERROR", str(e))
            return
        self.update_last_dir(path)
        self.refresh_config_data_page()

    def load_from_raw_bin(self):
        path = self.get_open_file_name("bin")
        if not path:
            return
        self.load_bin_file(path, False)

    def load_from_var_list_bin(self):
        path = self.get_open_file_name("bin")
        if not path:
            return
        self.load_bin_file(path, True)

    def load_from_svd(self):
        path = self.get_open_file_name("svd")
        if not path:
            return
        for idx in self.cfg_data_list:
            self.cfg_data_list[idx].cfg_data_obj.load_from_svd(path)
            self.refresh_config_data_page()

    def load_bin_file(self, path, is_variable_list_format):
        with open(path, "rb") as fd:
            bin_data = bytearray(fd.read())
        bin_len = 0
        for idx in self.cfg_data_list:
            bin_len += len(self.cfg_data_list[idx].org_cfg_data_bin)

        if len(bin_data) < bin_len:
            messagebox.showerror(
                "Binary file size is smaller than what YAML requires !"
            )
            return

        try:
            for idx in self.cfg_data_list:
                self.reload_config_data_from_bin(bin_data, idx, is_variable_list_format)
        except Exception as e:
            messagebox.showerror("LOADING ERROR", str(e))
            return

    def load_cfg_file(self, path, file_id):
        self.cfg_data_list[file_id] = cfg_data()

        # Set up the config type to begin with
        if path.lower().endswith('.xml'):
            self.cfg_data_list[file_id].config_type = 'xml'
        else:
            self.cfg_data_list[file_id].config_type = 'yml'

        # If not first config file, save current values in widget and clear database
        if file_id == 0:
            self.clear_widgets_inLayout()
            self.left.delete(*self.left.get_children())

        self.cfg_data_list[file_id].cfg_data_obj = self.load_config_data(path)

        self.update_last_dir(path)
        self.cfg_data_list[file_id].org_cfg_data_bin = self.cfg_data_list[file_id].cfg_data_obj.generate_binary_array()
        self.build_config_page_tree(self.cfg_data_list[file_id].cfg_data_obj.get_cfg_page()["root"], "", file_id)

        for menu in self.menu_string:
            if self.cfg_data_list[file_id].config_type == 'xml' and menu in self.yaml_specific_setting:
                # Block out the yaml options for loading for now
                self.file_menu.entryconfig(menu, state="disabled")
            else:
                self.file_menu.entryconfig(menu, state="normal")

        return 0

    def load_from_ml(self):
        path = self.get_open_file_name('yaml,yml,xml')
        if not path:
            return

        self.load_cfg_file(path, 0)

    def get_save_file_name(self, extension):
        path = filedialog.asksaveasfilename(
            initialdir=self.last_dir,
            title="Save file",
            defaultextension=extension,
            filetypes=((extension + " file", "*" + extension), ("All Files", "*.*")))
        if path:
            self.last_dir = os.path.dirname(path)
            return path
        else:
            return None

    def save_delta_file(self, full=False):
        path = self.get_save_file_name(".dlt")
        if not path:
            return

        dlt_path = path

        # assumption is there is one yaml file and one xml file
        # yml changes get saved to dlt, xml changes get saved to csv
        for file_id in self.cfg_data_list:
            self.update_config_data_on_page()

            if self.cfg_data_list[file_id].config_type == 'yml':
                dlt_path += '.dlt'
            else:
                dlt_path += '.csv'

            new_data = self.cfg_data_list[file_id].cfg_data_obj.generate_binary_array()
            self.cfg_data_list[file_id].cfg_data_obj.generate_delta_file_from_bin(
                dlt_path, self.cfg_data_list[file_id].org_cfg_data_bin, new_data, full
            )

    def save_to_svd(self):
        path = self.get_save_file_name(".svd")
        if not path:
            return

        base64_path = path
        temp_file = path + ".tmp"

        self.update_config_data_on_page()
        new_data = self.cfg_data_list[0].cfg_data_obj.generate_binary_array()

        (execs, bytes_array) = self.cfg_data_list[0].cfg_data_obj.generate_delta_svd_from_bin(
            self.cfg_data_list[0].org_cfg_data_bin, new_data
        )

        settings = []
        for index in range(len(execs)):
            b64data = base64.b64encode(bytes_array[index])
            # This should start with SINGLE_SETTING_PROVIDER_TEMPLATE from SetupDataPkg/Include/Library/ConfigDataLib.h
            cfg_hdr = self.cfg_data_list[0].cfg_data_obj.get_item_by_index(
                execs[index]["CfgHeader"]["indx"]
            )
            tag_val = array_str_to_value(cfg_hdr["value"]) >> 20
            settings.append(
                ("Device.ConfigData.TagID_%08x" % tag_val, b64data.decode("utf-8"))
            )
        set_lib = SettingsXMLLib()
        set_lib.create_settings_xml(
            filename=temp_file, version=1, lsv=1, settingslist=settings
        )

        # To remove the line ends and spaces
        with open(temp_file, "r") as tf:
            with open(base64_path, "w") as ff:
                for line in tf:
                    line = line.strip().rstrip("\n")
                    ff.write(line)
        os.remove(temp_file)

    def save_to_delta(self):
        self.save_delta_file()

    def save_full_to_delta(self):
        self.save_delta_file(True)

    def save_to_raw_bin(self):
        self.save_to_bin(False)

    def save_to_var_list_bin(self):
        self.save_to_bin(True)

    def save_to_bin(self, is_variable_list_format):
        path = self.get_save_file_name(".bin")
        if not path:
            return

        self.update_config_data_on_page()
        with open(path, "wb") as fd:
            bins = b''
            for idx in self.cfg_data_list:
                bin = None
                if self.cfg_data_list[idx].config_type == 'yml' and is_variable_list_format:
                    # need to get the yml svd, base64 encode it, stuff it in UEFI Var
                    # add that to a buffer, then add that to the bin
                    svd = self.save_full_to_svd(True)
                    bin = self.cfg_data_list[idx].cfg_data_obj.generate_var_list_from_svd(svd)
                else:
                    bin = self.cfg_data_list[idx].cfg_data_obj.generate_binary_array()

                bins += bin

            fd.write(bins)

    def save_full_to_svd(self, gen_yml_svd=False):
        path = None
        if gen_yml_svd:
            # if we are only generating a yml svd to save in the var list binary, we don't want to write this to a file
            path = 'svdtmp'
        else:
            path = self.get_save_file_name(".svd")
            if not path:
                return

        base64_path = path
        temp_file = path + ".tmp"
        settings = []

        self.update_config_data_on_page()

        for file_id in self.cfg_data_list:
            found_yml = False
            if gen_yml_svd:
                for idx in self.cfg_data_list:
                    if self.cfg_data_list[idx].config_type == 'yml':
                        file_id = idx
                        found_yml = True
                if not found_yml:
                    raise Exception('Failed to find YAML config to generate SVD from!')
            b64data = base64.b64encode(self.cfg_data_list[file_id].cfg_data_obj.generate_binary_array())
            # This should match DFCI_OEM_SETTING_ID__CONF from SetupDataPkg/Include/Library/ConfigDataLib.h
            if self.cfg_data_list[file_id].config_type == 'yml':
                settings.append(("Device.ConfigData.ConfigData", b64data.decode("utf-8")))
            else:
                settings.append(("Device.RuntimeData.RuntimeData", b64data.decode("utf-8")))

            if gen_yml_svd:
                break

        set_lib = SettingsXMLLib()
        set_lib.create_settings_xml(
            filename=temp_file, version=1, lsv=1, settingslist=settings
        )

        yml_svd = ''

        # To remove the line ends and spaces
        with open(temp_file, "r") as tf:
            if not gen_yml_svd:
                with open(base64_path, "w") as ff:
                    for line in tf:
                        line = line.strip().rstrip("\n")
                        ff.write(line)
            else:
                for line in tf:
                    yml_svd += line.strip().rstrip("\n")
        os.remove(temp_file)
        return yml_svd

    def refresh_config_data_page(self):
        self.clear_widgets_inLayout()
        self.on_config_page_select_change(None)

    def reload_config_data_from_bin(self, bin_dat, file_id, is_variable_list_format):
        self.cfg_data_list[file_id].cfg_data_obj.load_default_from_bin(bin_dat, is_variable_list_format)
        self.refresh_config_data_page()

    def set_config_item_value(self, item, value_str, file_id):
        itype = item["type"].split(",")[0]
        if itype == "Table":
            new_value = value_str
        elif itype == "EditText":
            length = (self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_length(item) + 7) // 8
            new_value = value_str[:length]
            if item["value"].startswith("'"):
                new_value = "'%s'" % new_value
        elif itype.upper() in ['FLOAT_KNOB', 'INTEGER_KNOB', 'ENUM_KNOB', 'BOOL_KNOB']:
            try:
                self.cfg_data_list[file_id].cfg_data_obj.set_item_value(value_str, item)
                new_value = self.cfg_data_list[file_id].cfg_data_obj.reformat_value_str(
                    value_str, self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_length(item), item=item
                )
            except:
                print("WARNING: Failed to format knob value string '%s' for '%s' !" % (value_str, item['path']))
                new_value = item['value']
        else:
            try:
                new_value = self.cfg_data_list[file_id].cfg_data_obj.reformat_value_str(
                    value_str,
                    self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_length(item),
                    item["value"],
                )
            except:
                print(
                    "WARNING: Failed to format value string '%s' for '%s' !"
                    % (value_str, item["path"])
                )
                new_value = item["value"]

        if item["value"] != new_value:
            if self.debug:
                print(
                    "Update %s from %s to %s !"
                    % (item["cname"], item["value"], new_value)
                )
            item["value"] = new_value

    def get_config_data_item_from_widget(self, widget, label=False):
        name, file_id = self.get_object_name(widget)
        if not name or not len(self.page_list):
            return None

        if name.startswith("LABEL_"):
            if label:
                path = name[6:]
            else:
                return None
        else:
            path = name

        item = self.cfg_data_list[file_id].cfg_data_obj.get_item_by_path(path)
        return item

    def update_config_data_from_widget(self, widget, args):
        item = self.get_config_data_item_from_widget(widget)
        if item is None:
            return
        elif not item:
            if isinstance(widget, tkinter.Label):
                return
            raise Exception('Failed to find "%s" !' % self.get_object_name(widget)[0])

        file_id = self.get_object_name(widget)[1]

        itype = item["type"].split(",")[0]
        if itype == "Combo":
            opt_list = self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_options(item)
            tmp_list = [opt[0] for opt in opt_list]
            idx = widget.current()
            self.set_config_item_value(item, tmp_list[idx], file_id)
        elif itype in ["EditNum", "EditText"]:
            self.set_config_item_value(item, widget.get(), file_id)
        elif itype in ["Table"]:
            new_value = bytes_to_bracket_str(widget.get())
            self.set_config_item_value(item, new_value, file_id)
        elif itype.upper() in ['ENUM_KNOB', 'BOOL_KNOB']:
            opt_list = self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_options(item)
            tmp_list = [opt for opt in opt_list]
            idx = widget.current()
            self.set_config_item_value(item, tmp_list[idx], file_id)
        elif itype.upper() in ['FLOAT_KNOB', 'INTEGER_KNOB']:
            self.set_config_item_value(item, widget.get(), file_id)

    def evaluate_condition(self, item, file_id):
        try:
            result = self.cfg_data_list[file_id].cfg_data_obj.evaluate_condition(item)
        except:
            print(
                "WARNING: Condition '%s' is invalid for '%s' !"
                % (item["condition"], item["path"])
            )
            result = 1
        return result

    def add_config_item(self, item, row, file_id):
        parent = self.right_grid

        name = tkinter.Label(parent, text=item["name"], anchor="w")

        parts = item["type"].split(",")
        itype = parts[0]
        widget = None

        if itype == "Combo":
            # Build
            opt_list = self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_options(item)
            current_value = self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_value(item, False)
            option_list = []
            current = None

            for idx, option in enumerate(opt_list):
                option_str = option[0]
                try:
                    option_value = self.cfg_data_list[file_id].cfg_data_obj.get_value(
                        option_str, len(option_str), False
                    )
                except:
                    option_value = 0
                    print(
                        'WARNING: Option "%s" has invalid format for "%s" !'
                        % (option_str, item["path"])
                    )
                if option_value == current_value:
                    current = idx
                option_list.append(option[1])

            widget = ttk.Combobox(parent, value=option_list, state="readonly")
            widget.bind("<<ComboboxSelected>>", self.combo_select_changed)
            widget.unbind_class("TCombobox", "<MouseWheel>")

            if current is None:
                print(
                    'WARNING: Value "%s" is an invalid option for "%s" !'
                    % (current_value, item["path"])
                )
            else:
                widget.current(current)

        elif itype in ["EditNum", "EditText"]:
            txt_val = tkinter.StringVar()
            widget = tkinter.Entry(parent, textvariable=txt_val)
            value = item["value"].strip("'")
            if itype in ["EditText"]:
                txt_val.trace(
                    "w",
                    lambda *args: self.limit_entry_size(
                        txt_val, (self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_length(item) + 7) // 8
                    ),
                )
            elif itype in ["EditNum"]:
                value = item["value"].strip("{").strip("}").strip()
                widget.bind("<FocusOut>", self.edit_num_finished)
            txt_val.set(value)

        elif itype in ["Table"]:
            bins = self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_value(item, True)
            col_hdr = item["option"].split(",")
            widget = custom_table(parent, col_hdr, bins)

        elif itype.upper() in ['FLOAT_KNOB', 'INTEGER_KNOB']:
            txt_val = tkinter.StringVar()
            widget = tkinter.Entry(parent, textvariable=txt_val)
            value = item['value'].strip("{").strip("}").strip()
            widget.bind("<FocusOut>", self.edit_num_finished)
            txt_val.set(value)

        elif itype.upper() in ['STRUCT_KNOB']:
            # These are only displayed as labels and show helper tags
            create_tool_tip(name, item['help'])
            self.set_object_name(name, 'LABEL_' + item['path'], file_id)
            name.config(font=('calibri', 12, 'bold'))
            name.grid(row=row, column=0, padx=10, pady=5, sticky="nsew")

        elif itype.upper() in ['ARRAY_KNOB']:
            # Do nothing, as the array members is flattened and come next
            widget = None

        elif itype.upper() in ['ENUM_KNOB', 'BOOL_KNOB']:
            # Build
            opt_list = self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_options(item)
            current_value = self.cfg_data_list[file_id].cfg_data_obj.get_cfg_item_value(item, False)
            option_list = []
            current = None

            for idx, option in enumerate(opt_list):
                option_str = option
                try:
                    option_value = self.cfg_data_list[file_id].cfg_data_obj.get_value(
                        item, len(option_str), False, option_str
                    )
                except:
                    option_value = 0
                    print('WARNING: Option "%s" has invalid format for "%s" !' % (option_str, item['path']))
                if option_value == current_value:
                    current = idx
                option_list.append(option)

            widget = ttk.Combobox(parent, value=option_list, state="readonly")
            widget.bind("<<ComboboxSelected>>", self.combo_select_changed)
            widget.unbind_class("TCombobox", "<MouseWheel>")

            if current is None:
                print('WARNING: Value "%s" is an invalid option for "%s" !' %
                      (current_value, item['path']))
            else:
                widget.current(current)

        else:
            if itype and itype not in ["Reserved"]:
                print(
                    "WARNING: Type '%s' is invalid for '%s' !" % (itype, item["path"])
                )

        if widget:
            create_tool_tip(widget, item["help"])
            self.set_object_name(name, "LABEL_" + item["path"], file_id)
            self.set_object_name(widget, item["path"], file_id)
            name.grid(row=row, column=0, padx=10, pady=5, sticky="nsew")
            widget.grid(
                row=row + 1, rowspan=1, column=0, padx=10, pady=5, sticky="nsew"
            )

    def update_config_data_on_page(self):
        self.walk_widgets_in_layout(
            self.right_grid, self.update_config_data_from_widget
        )


if __name__ == "__main__":
    root = tkinter.Tk()
    app = application(master=root)
    root.title("Config Editor")
    root.mainloop()
