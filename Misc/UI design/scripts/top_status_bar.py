import time
import interact as i #Mandatory import

def main():
    i.init() # Mandatory

    while True:
        # Clear current commands
        i.reset()
        # Commands
        i.CLEAR(1, 1, 1)
        #
        # This statement checks whether the script should continue or not.
        # Therefore, make sure to add this code statement in every loop.
        if i.execute() < 0: #Move current commands into RAM
            i.deinit()
            return

        time.sleep(1)

if __name__ == "__main__":
    main()