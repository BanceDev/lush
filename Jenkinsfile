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
        stage('Compile Project & Extract Artifacts') {
            steps {
                script {
                    // Define a unique name for our temporary container
                    def containerName = "lush-build-${BUILD_NUMBER}"
                    
                    try {
                        echo 'Compiling the Lush project inside a container...'
                        // Run the entire build process in a single, named container.
                        // This container will be stopped but not removed
                        // which preserves its filesystem for the next step
                        sh "docker run --name ${containerName} lush-app:latest /bin/bash -c 'premake5 gmake2 && make'"
                        
                        echo 'Extracting compiled binary from the container...'
                        // Copy the 'bin' directory
                        // from the container's filesystem to the Jenkins workspace.
                        sh "docker cp ${containerName}:/app/bin ./"
                    } finally {
                        echo "Cleaning up build container: ${containerName}"
                        sh "docker rm ${containerName} || true"
                    }
                }
            }
        }
        stage('Run Lua 5.2 Compatibility Test') {
            steps {
                script {
                    echo 'Running the Lua 5.2 compatibility test script...'
                    sh '''
                    cat <<'EOF' > test_52.lua
                    -- Lua 5.2-specific features
                    print("--- Running Lua 5.2 Compatibility Test ---")
                    local _ENV = { print = print, x = 123 } -- lexical environments
                    function show_x()
                        print("x =", x)
                    end
                    show_x()
                    -- Bitwise ops (added in 5.2)
                    local a, b = 0x5, 0x3
                    print("Bitwise AND:", a & b)
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
                    // bin should be in workspace
                    sh 'docker run --rm -v "$(pwd)":/work -w /work lush-app:latest ./bin/Debug/lush/lush test_52.lua'
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
