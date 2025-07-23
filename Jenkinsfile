pipeline {
    agent any

    stages {
        stage('Build Application Image') {
            steps {
                script {
                    echo 'Initializing Git submodules...'
                    // Manually initialize and update the git submodules
                    sh 'git submodule update --init --recursive'

                    echo 'Building the Lush application Docker image...'
                    sh 'docker build -t lush-app:latest .'
                }
            }
        }
        stage('Compile & Test Project') {
            steps {
                script {
                    // Define a unique name for our temporary build container
                    def containerName = "lush-build-${BUILD_NUMBER}"
                    
                    try {
                        echo 'Creating test script on Jenkins agent...'
                        sh '''
                        cat <<'EOF' > test_52.lua
                        -- Lua 5.2-specific features
                        print("--- Running Lua 5.2 Compatibility Test ---")
                        local _ENV = { print = print, x = 123 } -- lexical environments
                        function show_x()
                            print("x =", x)
                        end
                        show_x()
                        -- Bitwise ops (using the bit32 library provided by compat53)
                        local bit32 = require("bit32")
                        local a, b = 0x5, 0x3
                        print("Bitwise AND:", bit32.band(a, b))
                        -- load() replaces loadstring() (binary and text)
                        local f = load("return 10 + 20")
                        print("Loaded result:", f())
                        -- table.pack / table.unpack
                        local t = table.pack(1, 2, 3, nil, 5)
                        print("Packed length:", t.n)
                        print("Unpacked values:", table.unpack(t, 1, t.n))
                        print("--- Test Complete ---")
                        EOF
                        '''
                        echo 'Creating and starting the build container...'
                        // Create the container and keep it running in the background
                        sh "docker run -d --name ${containerName} lush-app:latest sleep infinity"

                        echo 'Copying test script into the container...'
                        sh "docker cp test_52.lua ${containerName}:/app/test_52.lua"

                        echo 'Compiling and running tests inside the container...'
                        sh "docker exec ${containerName} /bin/bash -c 'premake5 gmake2 && make && ./bin/Debug/lush/lush test_52.lua'"

                        echo 'Extracting compiled binary from the container...'
                        // This is good practice for storing build artifacts
                        sh "docker cp ${containerName}:/app/bin ./"

                    } finally {
                        // This block ensures the build container is always stopped and removed
                        echo "Cleaning up build container: ${containerName}"
                        sh "docker stop ${containerName} >/dev/null 2>&1 || true"
                        sh "docker rm ${containerName} >/dev/null 2>&1 || true"
                    }
                }
            }
        }
    }
    post {
        always {
            echo 'Pipeline finished.'
            sh 'docker rmi lush-app:latest || true'
        }
    }
}
