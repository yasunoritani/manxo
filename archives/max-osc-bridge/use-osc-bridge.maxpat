{
    "patcher" : 	{
        "fileversion" : 1,
        "appversion" : 		{
            "major" : 8,
            "minor" : 2,
            "revision" : 1,
            "architecture" : "x64",
            "modernui" : 1
        },
        "classnamespace" : "box",
        "rect" : [ 59.0, 106.0, 689.0, 665.0 ],
        "bglocked" : 0,
        "openinpresentation" : 0,
        "default_fontsize" : 12.0,
        "default_fontface" : 0,
        "default_fontname" : "Arial",
        "gridonopen" : 1,
        "gridsize" : [ 15.0, 15.0 ],
        "gridsnaponopen" : 1,
        "objectsnaponopen" : 1,
        "statusbarvisible" : 2,
        "toolbarvisible" : 1,
        "lefttoolbarpinned" : 0,
        "toptoolbarpinned" : 0,
        "righttoolbarpinned" : 0,
        "bottomtoolbarpinned" : 0,
        "toolbars_unpinned_last_save" : 0,
        "tallnewobj" : 0,
        "boxanimatetime" : 200,
        "enablehscroll" : 1,
        "enablevscroll" : 1,
        "devicewidth" : 0.0,
        "description" : "",
        "digest" : "",
        "tags" : "",
        "style" : "",
        "subpatcher_template" : "",
        "assistshowspatchername" : 0,
        "boxes" : [ 			{
                "box" : 				{
                    "id" : "obj-19",
                    "linecount" : 2,
                    "maxclass" : "comment",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 375.0, 372.0, 285.0, 33.0 ],
                    "text" : "↑ MCPサーバーからの応答が表示されます\n（/max/pong, /max/status, /max/response/testなど）"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-17",
                    "linecount" : 2,
                    "maxclass" : "comment",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 96.0, 330.0, 233.0, 33.0 ],
                    "text" : "← ここをクリックして各種テストメッセージを送信（サーバーのシミュレート）"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-15",
                    "maxclass" : "message",
                    "numinlets" : 2,
                    "numoutlets" : 1,
                    "outlettype" : [ "" ],
                    "patching_rect" : [ 44.0, 377.0, 95.0, 22.0 ],
                    "text" : "/mcp/status"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-14",
                    "maxclass" : "message",
                    "numinlets" : 2,
                    "numoutlets" : 1,
                    "outlettype" : [ "" ],
                    "patching_rect" : [ 44.0, 347.0, 95.0, 22.0 ],
                    "text" : "/mcp/test"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-13",
                    "maxclass" : "message",
                    "numinlets" : 2,
                    "numoutlets" : 1,
                    "outlettype" : [ "" ],
                    "patching_rect" : [ 44.0, 317.0, 95.0, 22.0 ],
                    "text" : "/mcp/ping"
                }

            },
            {
                "box" : 				{
                    "fontface" : 1,
                    "id" : "obj-12",
                    "maxclass" : "comment",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 21.0, 282.0, 217.0, 20.0 ],
                    "text" : "2. MCPシミュレーションテスト"
                }

            },
            {
                "box" : 				{
                    "fontface" : 1,
                    "id" : "obj-7",
                    "maxclass" : "comment",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 21.0, 52.0, 217.0, 20.0 ],
                    "text" : "1. OSC接続テスト"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-11",
                    "linecount" : 2,
                    "maxclass" : "comment",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 345.0, 195.0, 307.0, 33.0 ],
                    "text" : "このオブジェクトはOSCメッセージを送受信します。\n左のアウトレットは受信したメッセージを出力します。"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-10",
                    "linecount" : 4,
                    "maxclass" : "comment",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 340.0, 79.0, 315.0, 60.0 ],
                    "text" : "使用方法：\n1. \"connect\"メッセージを送信して接続を確立\n2. OSCメッセージを送受信（デフォルト：7400/7500ポート）\n3. \"disconnect\"メッセージを送信して切断"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-9",
                    "linecount" : 4,
                    "maxclass" : "comment",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 96.0, 245.0, 220.0, 60.0 ],
                    "text" : "※ JavaScriptを使用する場合は：\n1. jsオブジェクトを作成\n2. \"js max-simple-bridge.js\"を設定\n3. 自動的に接続が確立されます"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-8",
                    "maxclass" : "comment",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 21.0, 19.0, 310.0, 20.0 ],
                    "text" : "Max 7.x/8.x - Min-DevKit & Claude Desktop MCP ブリッジ"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-6",
                    "maxclass" : "message",
                    "numinlets" : 2,
                    "numoutlets" : 1,
                    "outlettype" : [ "" ],
                    "patching_rect" : [ 198.0, 85.0, 100.0, 22.0 ],
                    "text" : "disconnect"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-4",
                    "maxclass" : "message",
                    "numinlets" : 2,
                    "numoutlets" : 1,
                    "outlettype" : [ "" ],
                    "patching_rect" : [ 96.0, 85.0, 100.0, 22.0 ],
                    "text" : "connect"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-2",
                    "maxclass" : "newobj",
                    "numinlets" : 1,
                    "numoutlets" : 0,
                    "patching_rect" : [ 96.0, 199.0, 100.0, 22.0 ],
                    "text" : "print response"
                }

            },
            {
                "box" : 				{
                    "id" : "obj-1",
                    "maxclass" : "newobj",
                    "numinlets" : 1,
                    "numoutlets" : 2,
                    "outlettype" : [ "", "" ],
                    "patching_rect" : [ 96.0, 142.0, 230.0, 22.0 ],
                    "text" : "osc_bridge 7400 7500"
                }

            }
        ],
        "lines" : [ 			{
                "patchline" : 				{
                    "destination" : [ "obj-2", 0 ],
                    "source" : [ "obj-1", 0 ]
                }

            },
            {
                "patchline" : 				{
                    "destination" : [ "obj-1", 0 ],
                    "source" : [ "obj-13", 0 ]
                }

            },
            {
                "patchline" : 				{
                    "destination" : [ "obj-1", 0 ],
                    "source" : [ "obj-14", 0 ]
                }

            },
            {
                "patchline" : 				{
                    "destination" : [ "obj-1", 0 ],
                    "source" : [ "obj-15", 0 ]
                }

            },
            {
                "patchline" : 				{
                    "destination" : [ "obj-1", 0 ],
                    "source" : [ "obj-4", 0 ]
                }

            },
            {
                "patchline" : 				{
                    "destination" : [ "obj-1", 0 ],
                    "source" : [ "obj-6", 0 ]
                }

            }
        ],
        "dependency_cache" : [ 			{
                "name" : "osc_bridge.mxo",
                "type" : "iLaX"
            }
        ],
        "autosave" : 0
    }
}
