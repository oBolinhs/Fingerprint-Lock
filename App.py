from kivy.config import Config

# Set the window size to mimic a Pixel 7a (Portrait Mode)
# Note: This must be done BEFORE importing other Kivy modules
Config.set('graphics', 'width', '315')
Config.set('graphics', 'height', '700')
Config.set('graphics', 'resizable', False) # Optional: prevents resizing
Config.write()

import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
from kivymd.app import MDApp
from kivymd.uix.screen import Screen
from kivymd.uix.button import MDFlatButton, MDRectangleFlatButton, MDFloatingActionButton, MDIconButton
from kivymd.uix.label import MDLabel
from kivy.clock import Clock

from kivy.uix.screenmanager import ScreenManager
from kivy.graphics import Color, Rectangle, Line 

import paho.mqtt.client as mqtt

cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://smartlock-44b7d-default-rtdb.europe-west1.firebasedatabase.app/' 
})

ref = db.reference('/lockState')

BROKER = "broker.emqx.io"
topicState = "bernas/door/state"
authAdd = "bernas/door/addauth"
newConnection = "bernas/door/newcon"

client = mqtt.Client()


ref = db.reference('/lockState')

class MainScreen(Screen):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

        with self.canvas.before:
            Color(33/255, 150/255, 243/255, 1)
            #Desenhar o retangulo de cima
            self.rect = Rectangle(pos=(0, 0), size=(100, 100))
            Color(33/255, 150/255, 243/255, 1) # Yellow
            
        self.bind(size=self.update_shapes, pos=self.update_shapes)
        
        
        self.ind_text = MDLabel(
            text = "Bedroom Lock",
            font_style = "H6",
            bold = True,
            font_size = "60dp",
            halign = "center",
            pos_hint={"center_x": 0.5, "center_y": 0.93},
            theme_text_color="Custom",
            text_color=(255/255, 255/255, 255/255, 1))
        self.add_widget(self.ind_text)
        
        self.lock_btn = MDIconButton(
                icon = 'lock',
                size_hint=(None, None),
                size=("200dp", "200dp"),
                icon_size="125dp",
                theme_icon_color="Custom",
                icon_color="white",
                md_bg_color=(33/255, 150/255, 243/255, 1), 
                pos_hint={'center_x':0.5,'center_y':0.45},
                on_release = self.toggleLock)
        self.add_widget(self.lock_btn)

        #botão para aceder ao menu com mais funções
        self.config_menu_btn = MDIconButton(
            icon = 'view-grid',
            icon_size="40dp",
            theme_icon_color="Custom",
            icon_color = (33/255, 150/255, 243/255, 1),
            pos_hint={'center_x':0.3,'center_y':0.07},
            on_release = self.go_config
        )
        self.add_widget(self.config_menu_btn)

        #icone do home
        self.home_btn = MDIconButton(
            icon = 'home',
            size_hint=(None, None),
            size=("55dp", "55dp"),
            icon_size="40dp",
            theme_icon_color="Custom",
            icon_color="white",
            md_bg_color=(33/255, 150/255, 243/255, 1), 
            pos_hint={'center_x':0.7,'center_y':0.07})
        self.add_widget(self.home_btn)      

        self.start_fb_listener()

    def start_fb_listener(self):
        
        try:
            ref.listen(self.on_firebase_change)
        except Exception as e:
            print(f"Error starting listener: {e}")

    def on_firebase_change(self, event):
               
        print(f"Firebase Update Detected: {event.data}")
        Clock.schedule_once(lambda dt: self.update_btn(event.data))

    def update_btn(self, state):
        
        if state == True:
            if self.lock_btn.icon != 'lock-open-variant':
                self.lock_btn.icon = 'lock-open-variant' 
                print("UI Updated: UNLOCKED")
        else:
            if self.lock_btn.icon != 'lock':
                self.lock_btn.icon = 'lock'
                print("UI Updated: LOCKED")

    def toggleLock(self,instance):
         
        if self.lock_btn.icon != 'lock-open-variant':
            client.publish(topicState, "true")      #manda desbloquear a porta
        if self.lock_btn.icon != 'lock':
            client.publish(topicState, "false")     
        print("Unlocked!")

    def doorlock(self,instance):
        client.publish(topicState, "false")
        print("Locked")

    #ir para perfil
    def go_config(self,instance):

        self.manager.current = 'config_screen'
        self.manager.transition.direction = 'left'


    def update_shapes(self, *args):
        
        self.rect.pos = (0, self.height - 100)
        self.rect.size = (self.width, 100)
        

        
#menu de configurações          
class ConfigScreen(Screen):            
    def __init__(self, **kwargs):
        super().__init__(**kwargs)    
        with self.canvas.before:
            Color(33/255, 150/255, 243/255, 1)
            #Desenhar o retangulo de cima
            self.rect = Rectangle(pos=(0, 0), size=(100, 100))
            Color(33/255, 150/255, 243/255, 1) # Yellow
            
                
        self.bind(size=self.update_shapes, pos=self.update_shapes)        

        self.ind_text = MDLabel(
            text = "Bedroom Lock",
            font_style = "H6",
            bold = True,
            font_size = "60dp",
            halign = "center",
            pos_hint={"center_x": 0.5, "center_y": 0.93},
            theme_text_color="Custom",
            text_color=(255/255, 255/255, 255/255, 1))
        self.add_widget(self.ind_text)

        #Botão para aceder ao menu principal
        self.home_btn = MDIconButton(
            icon = 'home',
            icon_size="40dp",
            theme_icon_color="Custom",
            icon_color = (33/255, 150/255, 243/255, 1),
            pos_hint={'center_x':0.7,'center_y':0.07},
            on_release = self.go_home
            
        )
        self.add_widget(self.home_btn)

        #icone do config menu
        self.config_menu_btn = MDIconButton(
            icon = 'view-grid',
            size_hint=(None, None),
            size=("55dp", "55dp"),
            icon_size="40dp",
            theme_icon_color="Custom",
            icon_color="white",
            md_bg_color=(33/255, 150/255, 243/255, 1), 
            pos_hint={'center_x':0.3,'center_y':0.07})
        self.add_widget(self.config_menu_btn)

        

        self.new_fp_btn = MDRectangleFlatButton(
            text = 'New Fingerprint',          
            pos_hint = {'center_x':0.25,'center_y':0.7},
            on_release = self.new_fp
        )
        self.add_widget(self.new_fp_btn)

        self.new_kp_btn = MDRectangleFlatButton(
            text = '   Reset Pin   ',          
            pos_hint = {'center_x':0.75,'center_y':0.7},
            on_release = self.new_kp
        )
        self.add_widget(self.new_kp_btn)

        self.del_fp_btn = MDRectangleFlatButton(
            text = 'Delete Fingerprint',          
            pos_hint = {'center_x':0.27,'center_y':0.6},
            on_release = self.del_fp
        )
        self.add_widget(self.del_fp_btn)


        
    #ir para ecrã de configuração
    def go_home(self,instance):
        self.manager.current = 'main_screen'
        self.manager.transition.direction = 'left'
    def new_fp(self,instance):
        client.publish(authAdd,"fp")
        print("New fingerprint requested")  
        

    def new_kp(self,instance):
        client.publish(authAdd,"kp")
        print("Reseting pin")

    def del_fp(self,instance):
        client.publish(authAdd,"dfp")
        print("Deleting fingerprint")

    def update_shapes(self, *args):
        
        self.rect.pos = (0, self.height - 100)
        self.rect.size = (self.width, 100) 
 

class SmartLockApp(MDApp):
    
    def build(self):
        
        self.sm = ScreenManager()
        
        # Adicionar ecrãs
        self.main_screen = MainScreen(name='main_screen')
        self.config_screen = ConfigScreen(name='config_screen')
        

        self.sm.add_widget(self.main_screen)
        self.sm.add_widget(self.config_screen)
        
        return self.sm
    def on_start(self):
        
        client.on_connect = self.on_connect
        
        print("Connecting to Broker...")
        client.connect(BROKER, 1883, 60)
        
        
        client.loop_start()
    def on_connect(self, client, userdata, flags, rc):
        print(f"Connected! (Result: {rc})")
        client.subscribe(topicState)

    

SmartLockApp().run()             